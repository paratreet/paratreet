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
  CkVec< CkVec<pqSmoothNode> > neighbors;
  CkVec< CkVec<Particle> > fixed_ball;
  OrientedBox<Real> box;
  int count;
  Real rsq;
  static constexpr const double theta = 0.7;

  CentroidData() :
  moment(Vector3D<Real> (0,0,0)), sum_mass(0), count(0), rsq(0.) {}

  CentroidData(const Particle* particles, int n_particles) : CentroidData() {
    for (int i = 0; i < n_particles; i++) {
      moment += particles[i].mass * particles[i].position;
      sum_mass += particles[i].mass;
      box.grow(particles[i].position);
    }
    centroid = moment / sum_mass;
    size_sm = 0.5*(box.size()).length();
    count += n_particles;
    fixed_ball.resize(n_particles);
    neighbors.resize(n_particles);
  }

  const CentroidData& operator+=(const CentroidData& cd) { // needed for upward traversal
    moment += cd.moment;
    sum_mass += cd.sum_mass;
    centroid = moment / sum_mass;
    box.grow(cd.box);
    Vector3D<Real> delta1 = centroid - box.lesser_corner;
    Vector3D<Real> delta2 = box.greater_corner - centroid;
    delta1.x = (delta1.x > delta2.x ? delta1.x : delta2.x);
    delta1.y = (delta1.y > delta2.y ? delta1.y : delta2.y);
    delta1.z = (delta1.z > delta2.z ? delta1.z : delta2.z);
    rsq = delta1.lengthSquared() / theta;

    size_sm = 0.5*(box.size()).length();
    max_rad = max_rad > cd.max_rad ? max_rad : cd.max_rad;

    for (int i = 0; i < cd.fixed_ball.size(); i++) {
      fixed_ball.insertAtEnd(cd.fixed_ball[i]);
    }

    for (int i = 0; i < cd.neighbors.size(); i++) {
      neighbors.insertAtEnd(cd.neighbors[i]);
    }

    count += cd.count;
    return *this;
  }

  const CentroidData& operator= (const CentroidData& cd) {
    moment = cd.moment;
    sum_mass = cd.sum_mass;
    centroid = cd.centroid;
    box = cd.box;
    count = cd.count;
    rsq = cd.rsq;
    max_rad = cd.max_rad;
    size_sm = cd.size_sm;
    fixed_ball = cd.fixed_ball;
    neighbors = cd.neighbors;
    return *this;
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
    p | fixed_ball;
    p | neighbors;
  }

};

#endif // PARATREET_CENTROID_H_
