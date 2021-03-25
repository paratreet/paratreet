#ifndef PARATREET_CENTROIDDATA_H_
#define PARATREET_CENTROIDDATA_H_

#include "common.h"
#include <vector>
#include <queue>
#include "Particle.h"
#include "ParticleComp.h"
#include "OrientedBox.h"

struct CentroidData {
  Vector3D<Real> moment;
  Real sum_mass;
  Vector3D<Real> centroid; // too slow to compute this on the fly
  Real max_rad = 0.0;
  Real size_sm;
  std::vector< CkVec<pqSmoothNode> > neighbors; // Neighbor list for knn search
  std::vector<std::pair<Real, Particle>> best_dt;
  OrientedBox<Real> box;
  int count;
  Real rsq;
  static constexpr const Real opening_geometry_factor_squared = 4.0 / 3.0;
  static constexpr const Real theta = 0.7;

  CentroidData() :
  moment(Vector3D<Real> (0,0,0)), sum_mass(0), count(0), rsq(0.) {}

  CentroidData(const Particle* particles, int n_particles) : CentroidData() {
    for (int i = 0; i < n_particles; i++) {
      moment += particles[i].mass * particles[i].position;
      sum_mass += particles[i].mass;
      box.grow(particles[i].position);
    }
    centroid = moment / sum_mass;
    getRadius();
    count = n_particles;
  }

  void getRadius() {
    Vector3D<Real> delta1 = centroid - box.lesser_corner;
    Vector3D<Real> delta2 = box.greater_corner - centroid;
    delta1.x = (delta1.x > delta2.x ? delta1.x : delta2.x);
    delta1.y = (delta1.y > delta2.y ? delta1.y : delta2.y);
    delta1.z = (delta1.z > delta2.z ? delta1.z : delta2.z);
    rsq = delta1.lengthSquared() * opening_geometry_factor_squared / (theta * theta);
    size_sm = 0.5*(box.size()).length();
  }

  const CentroidData& operator+=(const CentroidData& cd) { // needed for upward traversal
    moment += cd.moment;
    sum_mass += cd.sum_mass;
    centroid = moment / sum_mass;
    box.grow(cd.box);
    getRadius();
    count += cd.count;
    return *this;
  }

  CentroidData& operator=(const CentroidData&) = default;

  void widen() {
    neighbors.resize(count);
    best_dt.resize(count, std::make_pair(std::numeric_limits<Real>::max(), Particle{}));
  }

  void pup(PUP::er& p) {
    p | moment;
    p | sum_mass;
    p | centroid;
    p | box;
    p | count;
    p | rsq;
    p | max_rad;
    p | size_sm;
  }

};

#endif // PARATREET_CENTROID_H_
