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
  std::vector< std::priority_queue<Particle, std::vector<Particle>, particle_comp> > neighbors; // used for sph
  OrientedBox<Real> box;
  int count;
  Real rsq;

  CentroidData() :
  moment(Vector3D<Real> (0,0,0)), sum_mass(0), count(0), rsq(0.) {}

  CentroidData(Particle* particles, int n_particles) : CentroidData() {
    for (int i = 0; i < n_particles; i++) {
      moment += particles[i].mass * particles[i].position;
      sum_mass += particles[i].mass;
      box.grow(particles[i].position);
    }
    count += n_particles;
  }

  const CentroidData& operator+=(const CentroidData& cd) { // needed for upward traversal
    moment += cd.moment;
    sum_mass += cd.sum_mass;
    box.grow(cd.box);
    Vector3D<Real> delta1 = getCentroid() - box.lesser_corner;
    Vector3D<Real> delta2 = box.greater_corner - getCentroid();
    delta1.x = (delta1.x > delta2.x ? delta1.x : delta2.x);
    delta1.y = (delta1.y > delta2.y ? delta1.y : delta2.y);
    delta1.z = (delta1.z > delta2.z ? delta1.z : delta2.z);
    rsq = delta1.lengthSquared();

    count += cd.count;
    return *this;
  }
  const CentroidData& operator= (const CentroidData& cd) {
    moment = cd.moment;
    sum_mass = cd.sum_mass;
    box = cd.box;
    count = cd.count;
    rsq = cd.rsq;
    return *this;
  }
  void pup(PUP::er& p) {
    p | moment;
    p | sum_mass;
    p | box;
    p | count;
    p | rsq;
  }
  Vector3D<Real> getCentroid() const {
    return moment / sum_mass;
  }
};

#endif // PARATREET_CENTROID_H_
