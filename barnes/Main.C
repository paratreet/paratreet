#include "Main.decl.h"
#include "Paratreet.h"
#include "MultipoleMoments.h"
#include "BasicParticle.h"
#include "BasicBoundingBox.h"
#include "Space.h"
#include "TipsyFile.h"

/*readonly*/ Real theta;
/*readonly*/ Real max_timestep;
/*readonly*/ std::string test_file;

struct Particle : paratreet::BasicParticle {
  Real soft = 0.;
  void pup(PUP::er &p);
};

void Particle::pup(PUP::er &p) {
  BasicParticle::pup(p);
  p | soft;
}

struct CentroidData {
  using Particle = ::Particle;
  using BoundingBox = paratreet::BasicBoundingBox;
  static void loadParticlesFromFile(int reader_index, int n_readers, const paratreet::Configuration& config, std::vector<Particle>& particles);
  static void addParticleToBox(const Particle& p, BoundingBox& box);
  static void adjustParticleForUniverse(Particle& p, const BoundingBox& box);
  static void outputToFile(int writer_index, int particle_index, const BoundingBox& box, int iter, const std::string& output_file, const std::vector<Particle>& particles, int indicator);

  MultipoleMoments multipoles;
  OrientedBox<Real> box;
  int count = 0;

  CentroidData() = default;
  /// Construct centroid from particles.
  CentroidData(const Particle* particles, int n_particles, int depth) : CentroidData() {
    for (int i = 0; i < n_particles; i++) {
      multipoles += particles[i];
      box.grow(particles[i].position);
    }
    count = n_particles;
    if(count > 1) {
      calculateRadiusBox(multipoles, box);
    }
    else {
      multipoles.radius = 1.0; // single particle boxes don't need scaling.
    }
  }

  const CentroidData& operator+=(const CentroidData& cd) { // needed for upward traversal
    box.grow(cd.box);
    multipoles += cd.multipoles;
    count += cd.count;
    if(count > 1) {
      calculateRadiusFarthestCorner(multipoles, box);
    }
    else {
      multipoles.radius = 1.0; // Single particle boxes don't need scaling
    }
    return *this;
  }

  CentroidData& operator=(const CentroidData&) = default;

  void pup(PUP::er& p) {
    p | multipoles;
    p | box;
    p | count;
  }
};

void CentroidData::loadParticlesFromFile(int reader_index, int n_readers, const paratreet::Configuration& config, std::vector<Particle>& particles) {

  static constexpr const Real gasConstant = 1.0;
  static constexpr const Real gammam1 = 5.0/3.0 - 1;
  static constexpr const Real meanMolWeight = 1.0;

  // Open tipsy file
  Tipsy::TipsyReader r(config.input_file);
  if (!r.status()) {
    CkPrintf("Reader %d failed to open tipsy file %s\n", reader_index, config.input_file.c_str());
    CkAbort("Tipsy reading failure in Reader -- see stdout");
  }

  // Read header and count particles
  Tipsy::header tipsyHeader = r.getHeader();
  int n_total = tipsyHeader.nbodies;
  int n_sph = tipsyHeader.nsph;
  int n_dark = tipsyHeader.ndark;
  int n_star = tipsyHeader.nstar;

  int n_particles = n_total / n_readers;
  int excess = n_total % n_readers;
  unsigned int start_particle = n_particles * reader_index;
  if (reader_index < (unsigned int)excess) {
    n_particles++;
    start_particle += reader_index;
  } else {
    start_particle += excess;
  }

  // Reserve space
  particles.resize(n_particles);

  // Read particles and grow bounding box
  if (!r.seekParticleNum(start_particle)) {
    CkAbort("Could not seek to particle\n");
  }

  Tipsy::gas_particle gp;
  Tipsy::dark_particle dp;
  Tipsy::star_particle sp;

  for (unsigned int i = 0; i < n_particles; i++) {
    if (start_particle + i < (unsigned int)n_sph) {
      if (!r.getNextGasParticle(gp)) {
        CkAbort("Could not read gas particle\n");
      }
      particles[i].mass = gp.mass;
      particles[i].soft = gp.hsmooth;
      particles[i].position = gp.pos;
      particles[i].velocity = gp.vel;
    }
    else if (start_particle + i < (unsigned int)n_sph + (unsigned int)n_dark) {
      if (!r.getNextDarkParticle(dp)) {
        CkAbort("Could not read dark particle\n");
      }
      particles[i].mass = dp.mass;
      particles[i].position = dp.pos;
      particles[i].velocity = dp.vel;
      particles[i].soft = dp.eps;
    }
    else {
      if (!r.getNextStarParticle(sp)) {
        CkAbort("Could not read star particle\n");
      }
      particles[i].mass = sp.mass;
      particles[i].position = sp.pos;
      particles[i].velocity = sp.vel;
      particles[i].soft = 0.;
    }
    particles[i].updated_time = tipsyHeader.time;
    particles[i].order = start_particle + i;
  }
}

void CentroidData::addParticleToBox(const Particle& p, BoundingBox& box) {
  box.n_particles++;
  box.grow(p.position);
  box.mass += p.mass;
  box.updated_time = std::max(box.updated_time, p.updated_time);
}

void CentroidData::adjustParticleForUniverse(Particle& p, const BoundingBox& box) {
  p.adjustForUniverse(box.boxCorners());
}

void CentroidData::outputToFile(int writer_index, int particle_index, const BoundingBox& box, int iter, const std::string& output_file, const std::vector<Particle>& particles, int indicator) {
  // Write particle accelerations to output file
  FILE *fp;
  if (writer_index == 0 && indicator == 0) {
    fp = CmiFopen((output_file+".acc").c_str(), "w");
    fprintf(fp, "%d\n", box.n_particles);
  } else {
    fp = CmiFopen((output_file+".acc").c_str(), "a");
  }
  CkAssert(fp);

  for (const auto& particle : particles) {
    Real outval;
    if (indicator == 0) outval = particle.acceleration.x;
    else if (indicator == 1) outval = particle.acceleration.y;
    else if (indicator == 2) outval = particle.acceleration.z;
    fprintf(fp, "%.14g\n", outval);
  }

  int result = CmiFclose(fp);
  CkAssert(result == 0);
}

inline Real COSMO_CONST(const Real C) {return C;}
/// Calculate softened force and potential terms from cubic spline
/// density profiles.  Terms are returned in a and b.
/// 
inline
void SPLINE(Real r2, Real twoh, Real &a, Real &b)
{
  auto r = sqrt(r2);

  if (r < twoh) {
    auto dih = COSMO_CONST(2.0)/twoh;
    auto u = r*dih;
    if (u < COSMO_CONST(1.0)) {
      a = dih*(COSMO_CONST(7.0)/COSMO_CONST(5.0) 
	       - COSMO_CONST(2.0)/COSMO_CONST(3.0)*u*u 
	       + COSMO_CONST(3.0)/COSMO_CONST(10.0)*u*u*u*u
	       - COSMO_CONST(1.0)/COSMO_CONST(10.0)*u*u*u*u*u);
      b = dih*dih*dih*(COSMO_CONST(4.0)/COSMO_CONST(3.0) 
		       - COSMO_CONST(6.0)/COSMO_CONST(5.0)*u*u 
		       + COSMO_CONST(1.0)/COSMO_CONST(2.0)*u*u*u);
    }
    else {
      auto dir = COSMO_CONST(1.0)/r;
      a = COSMO_CONST(-1.0)/COSMO_CONST(15.0)*dir 
	+ dih*(COSMO_CONST(8.0)/COSMO_CONST(5.0) 
	       - COSMO_CONST(4.0)/COSMO_CONST(3.0)*u*u + u*u*u
	       - COSMO_CONST(3.0)/COSMO_CONST(10.0)*u*u*u*u 
	       + COSMO_CONST(1.0)/COSMO_CONST(30.0)*u*u*u*u*u);
      b = COSMO_CONST(-1.0)/COSMO_CONST(15.0)*dir*dir*dir 
	+ dih*dih*dih*(COSMO_CONST(8.0)/COSMO_CONST(3.0) - COSMO_CONST(3.0)*u 
		       + COSMO_CONST(6.0)/COSMO_CONST(5.0)*u*u 
		       - COSMO_CONST(1.0)/COSMO_CONST(6.0)*u*u*u);
    }
  }
  else {
    a = COSMO_CONST(1.0)/r;
    b = a*a*a;
  }
}

class GravityVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;
  static constexpr const bool ForceEvenDepth = true;
  static constexpr const bool TargetMustBeLeaf = true;
  static constexpr const Real opening_geometry_factor_squared = 4.0 / 3.0;
  GravityVisitor() : offset(0, 0, 0) {}
  GravityVisitor(Vector3D<Real> offseti, Real theta) :
    offset(offseti),
    gravity_factor(opening_geometry_factor_squared / (theta * theta)),
    monopole_gravity_factor(sqrt(opening_geometry_factor_squared) / (theta * theta * theta * theta))
  {}

  void pup(PUP::er& p) {
    p | offset;
    p | gravity_factor;
    p | monopole_gravity_factor;
  }

private:
  Vector3D<Real> offset;
  Real gravity_factor;
  Real monopole_gravity_factor;

private:
  // note gconst = 1
  static constexpr int  nMinParticleNode = 6;

  void addGravity(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      auto& part = target.particle(i);
      Vector3D<Real> diff = source.data.multipoles.cm + offset - part.position;
      Real rsq = diff.lengthSquared();
      if (rsq != 0) {
        Vector3D<Real> accel = diff * (source.data.multipoles.totalMass / (rsq * sqrt(rsq)));
        part.acceleration += accel;
      }
    }
  }

public:
  /// @brief We've hit a leaf: N^2 interactions between all particles
  /// in the target and node.
  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      auto& part = target.particle(i);
      for (int j = 0; j < source.n_particles; j++) {
          Vector3D<Real> diff = source.particles()[j].position + offset - part.position;
          Real rsq = diff.lengthSquared();
          Real twoh = source.particles()[j].soft + part.soft;
          if (rsq != 0) {
              Real a, b;        /* potential and force terms returned
                                 * from SPLINE */
              SPLINE(rsq, twoh, a, b);
              part.acceleration += diff * (b * source.particles()[j].mass);
          }
      }
    }
  }

  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.data.count <= nMinParticleNode) return true;
    Real dataRsq = source.data.multipoles.radius * source.data.multipoles.radius * gravity_factor;
    return Space::intersect(target.data.box, source.data.multipoles.cm + offset, dataRsq);
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.data.count == 0) return;
    addGravity(source, target);
  }

  bool cell(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // cell means: do we want to open up target
    // for example, if source is root, we dont want to open up target
    return !Space::enclose(source.data.box, target.data.box);
  }
};

struct AstroConfiguration : public paratreet::Configuration {
  Real theta;
  Real max_timestep;
  std::string test_file;
  AstroConfiguration();
  AstroConfiguration(CkMigrateMessage *m);
  PUPable_decl_inside(AstroConfiguration);
  virtual void pup(PUP::er &p) override;
};

AstroConfiguration::AstroConfiguration(): paratreet::Configuration() {
  this->register_field("achOutputFile", "v", test_file);
  this->register_field("dTheta", nullptr, theta);
  this->register_field("dMaxTimestep", "j", max_timestep);
}

AstroConfiguration::AstroConfiguration(CkMigrateMessage *m): paratreet::Configuration(m) {
}

void AstroConfiguration::pup(PUP::er &p) {
  paratreet::Configuration::pup(p);
  p | theta;
  p | max_timestep;
  p | test_file;
}

class ExMain: public paratreet::Main<CentroidData, AstroConfiguration> {
  virtual Real getTimestep(const paratreet::BasicBoundingBox&, Real) override;
  virtual void preTraversalFn(ProxyPack<CentroidData>&) override;
  virtual void traversalFn(const paratreet::BasicBoundingBox&, ProxyPack<CentroidData>&, int) override;
  virtual void postIterationFn(const paratreet::BasicBoundingBox&, ProxyPack<CentroidData>&, int) override;
  virtual void setDefaults(void) override;
  virtual void main(CkArgMsg*) override;
  virtual void run(void) override;
};

PARATREET_REGISTER_MAIN(ExMain);

static void initialize() {
  paratreet::BasicBoundingBox::registerReducer();
}

void ExMain::setDefaults(void) {
  conf.min_n_subtrees = CkNumPes() * 8; // default from ChaNGa
  conf.min_n_partitions = CkNumPes() * 8;
  conf.max_particles_per_leaf = 12; // default from ChaNGa
  conf.decomp_type = paratreet::DecompType::eBinaryOct;
  conf.tree_type = paratreet::TreeType::eBinaryOct;
  conf.num_iterations = 3;
  conf.num_share_nodes = 0; // 3;
  conf.cache_share_depth = 3;
  conf.pool_elem_size;
  conf.flush_period = 0;
  conf.flush_max_avg_ratio = 10.;
  conf.lb_period = 5;
  conf.request_pause_interval = 20;
  conf.iter_pause_interval = 100;
  conf.peanoKey = 3;
}

void ExMain::main(CkArgMsg* m) {
  // Print configuration
  CkPrintf("\n[PARATREET]\n");
  if (conf.input_file.empty()) CkAbort("Input file unspecified");
  CkPrintf("Input file: %s\n", conf.input_file.c_str());
  CkPrintf("Decomposition type: %s\n", paratreet::asString(conf.decomp_type).c_str());
  CkPrintf("Tree type: %s\n", paratreet::asString(conf.tree_type).c_str());
  CkPrintf("Minimum number of subtrees: %d\n", conf.min_n_subtrees);
  CkPrintf("Minimum number of partitions: %d\n", conf.min_n_partitions);
  CkPrintf("Maximum number of particles per leaf: %d\n", conf.max_particles_per_leaf);

  theta = conf.theta;
  max_timestep = conf.max_timestep;
  test_file = conf.test_file;

  // Delegate to Driver
  // CkCallback runCB(CkIndex_Main::run(), thisProxy);
  // driver = paratreet::initialize<CentroidData>(conf, runCB);
}

void ExMain::run() {
  driver.run(CkCallbackResumeThread());

  CkExit();
}

void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
  //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
  //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
  proxy_pack.driver.loadCache(CkCallbackResumeThread());
}

void ExMain::traversalFn(const paratreet::BasicBoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
  proxy_pack.partition.template startDown<GravityVisitor>(GravityVisitor(Vector3D<Real>(0, 0, 0), theta));
}

void ExMain::postIterationFn(const paratreet::BasicBoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
  if (iter == 0 && !test_file.empty()) {
    paratreet::outputSorted(test_file, universe, proxy_pack, iter, std::vector<int>{0, 1, 2});
  }
}

Real ExMain::getTimestep(const paratreet::BasicBoundingBox& universe, Real max_velocity) {
  Real universe_box_len = universe.box.greater_corner.x - universe.box.lesser_corner.x;
  Real temp = universe_box_len / max_velocity / std::cbrt(universe.n_particles);
  return std::min(temp, max_timestep);
}

// #include "paratreet.def.h"
#include "templates.h"

#include "Main.def.h"

