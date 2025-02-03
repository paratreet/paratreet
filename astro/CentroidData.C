#include "CentroidData.h"
#include "TipsyFile.h"
#include "AstroFields.h"

/* readonly */ extern AstroFields astroConf;

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

  bool setSoft = config.origin_of("dSoft") != paratreet::FieldOrigin::Unknown;
  if (setSoft) {
    CkPrintf("Setting softening to %f \n", astroConf.dSoft);
  }
  for (unsigned int i = 0; i < n_particles; i++) {
    particles[i].potential = 0.;
    particles[i].u = 0.;
    if (start_particle + i < (unsigned int)n_sph) {
      if (!r.getNextGasParticle(gp)) {
        CkAbort("Could not read gas particle\n");
      }
      particles[i].mass = gp.mass;
      particles[i].soft = setSoft ? astroConf.dSoft : gp.hsmooth;
      particles[i].position = gp.pos;
      particles[i].velocity = gp.vel;
      particles[i].u = gp.temp * gasConstant / gammam1 / meanMolWeight;
      particles[i].type = Particle::Type::eGas;
    }
    else if (start_particle + i < (unsigned int)n_sph + (unsigned int)n_dark) {
      if (!r.getNextDarkParticle(dp)) {
        CkAbort("Could not read dark particle\n");
      }
      particles[i].mass = dp.mass;
      particles[i].position = dp.pos;
      particles[i].velocity = dp.vel;
      particles[i].soft = setSoft ? astroConf.dSoft : dp.eps;
      particles[i].type = Particle::Type::eDark;
    }
    else {
      if (!r.getNextStarParticle(sp)) {
        CkAbort("Could not read star particle\n");
      }
      particles[i].mass = sp.mass;
      particles[i].position = sp.pos;
      particles[i].velocity = sp.vel;
      particles[i].type = Particle::Type::eStar;
      particles[i].soft = setSoft ? astroConf.dSoft : 0.;
    }
    particles[i].updated_time = tipsyHeader.time;
    particles[i].order = start_particle + i;
    particles[i].velocity_predicted = particles[i].velocity;
    particles[i].u_predicted = particles[i].u;
    particles[i].ball = 2.0*particles[i].velocity.length()*astroConf.max_timestep + (4*particles[i].soft);
  }
}

void CentroidData::addParticleToBox(const Particle& p, BoundingBox& box) {
  if (p.isGas()) box.n_sph++;
  if (p.isDark()) box.n_dark++;
  if (p.isStar()) box.n_star++;
  box.n_particles++;
  box.grow(p.position);
  box.mass += p.mass;
  box.ke += 0.5 * p.mass * p.velocity.lengthSquared();
  box.pe += p.potential;
  box.updated_time = std::max(box.updated_time, p.updated_time);
}

void CentroidData::adjustParticleForUniverse(Particle& p, const BoundingBox& box) {
  p.adjustForUniverse(box.boxCorners());
}

void CentroidData::outputToFile(int writer_index, int particle_index, const BoundingBox& box, int iter, const std::string& output_file, const std::vector<Particle>& particles, int indicator) {
  if (indicator == 3) {
    Tipsy::header tipsyHeader;

    tipsyHeader.time = box.updated_time;
    tipsyHeader.nbodies = box.n_particles;
    tipsyHeader.nsph = box.n_sph;
    tipsyHeader.ndark = box.n_dark;
    tipsyHeader.nstar = box.n_star;

    bool use_double = sizeof(Real) == 8;

    auto output_filename = output_file + "." + std::to_string(iter) + ".tipsy";

    if (writer_index == 0) CmiFopen(output_filename.c_str(), "w");

    Tipsy::TipsyWriter w(output_filename, tipsyHeader, false, use_double, use_double);

    if(writer_index == 0) w.writeHeader();

    if(!w.seekParticleNum(particle_index)) {
      CkPrintf("seeking %d particles for total %d gas %d dark %d star %d\n",
        particle_index, box.n_particles, box.n_sph, box.n_dark, box.n_star);
      CkAbort("bad seek");
    }

    for (const auto& p : particles) {
      if (p.isGas()) {
        Tipsy::gas_particle_t<Real, Real> gp;
        gp.mass = p.mass;
        gp.pos = p.position;
        gp.vel = p.velocity; // dvFac = 1
        if(!w.putNextGasParticle_t(gp)) {
          CkError("[%d] Write gas failed, errno %d: %s\n", CkMyPe(), errno, strerror(errno));
          CkAbort("Bad Write");
        }
      }
      else if (p.isDark()) {
        Tipsy::dark_particle_t<Real, Real> dp;
        dp.mass = p.mass;
        dp.pos = p.position;
        dp.vel = p.velocity; // dvFac = 1
        if(!w.putNextDarkParticle_t(dp)) {
          CkError("[%d] Write dark failed, errno %d: %s\n", CkMyPe(), errno, strerror(errno));
          CkAbort("Bad Write");
        }
      }
      else if (p.isStar()) {
        Tipsy::star_particle_t<Real, Real> sp;
        sp.mass = p.mass;
        sp.pos = p.position;
        sp.vel = p.velocity; // dvFac = 1
        if(!w.putNextStarParticle_t(sp)) {
          CkError("[%d] Write star failed, errno %d: %s\n", CkMyPe(), errno, strerror(errno));
          CkAbort("Bad Write");
        }
      }
      else {
        CkAbort("Unknown particle type\n");
      }
    }
  }
  else {
    // Write particle accelerations to output file
    FILE *fp;
    FILE *fpDen;
    FILE *fpPres;
    if (writer_index == 0 && indicator == 0) {
      fp = CmiFopen((output_file+".acc").c_str(), "w");
      fprintf(fp, "%d\n", box.n_particles);
      fpDen = CmiFopen((output_file+".den").c_str(), "w");
      fprintf(fpDen, "%d\n", box.n_particles);
      fpPres = CmiFopen((output_file+".pres").c_str(), "w");
      fprintf(fpPres, "%d\n", box.n_particles);
    } else {
      fp = CmiFopen((output_file+".acc").c_str(), "a");
      fpDen = CmiFopen((output_file+".den").c_str(), "a");
      fpPres = CmiFopen((output_file+".pres").c_str(), "a");
    }
    CkAssert(fp);
    CkAssert(fpDen);
    CkAssert(fpPres);

    for (const auto& particle : particles) {
      Real outval;
      if (indicator == 0) outval = particle.acceleration.x;
      else if (indicator == 1) outval = particle.acceleration.y;
      else if (indicator == 2) outval = particle.acceleration.z;
      fprintf(fp, "%.14g\n", outval);
      if (indicator == 0) {
        fprintf(fpDen, "%.14g\n", particle.density);
        const double gammam1 = 5./3. - 1.0;
        fprintf(fpPres, "%.14g\n", gammam1*particle.u*particle.density);
      }
    }

    int result = CmiFclose(fp);
    CkAssert(result == 0);
    result = CmiFclose(fpDen);
    CkAssert(result == 0);
    result = CmiFclose(fpPres);
    CkAssert(result == 0);
  }
}
