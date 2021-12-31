#ifndef PARATREET_SEARCHDATA_H_
#define PARATREET_SEARCHDATA_H_

#include "common.h"
#include <vector>
#include <queue>
#include "Particle.h"
#include "OrientedBox.h"

struct SearchData {
  Vector3D<Real> moment;
  Real sum_mass;
  Vector3D<Real> centroid; // too slow to compute this on the fly
  OrientedBox<Real> box;
  int count;

  SearchData() :
  moment(Vector3D<Real> (0,0,0)), sum_mass(0), count(0) {}

  /// Construct centroid from particles.
  SearchData(const Particle* particles, int n_particles, int depth) : SearchData() {
    for (int i = 0; i < n_particles; i++) {
      moment += particles[i].mass * particles[i].position;
      sum_mass += particles[i].mass;
      box.grow(particles[i].position);
    }
    centroid = moment / sum_mass;
    count = n_particles;
  }

  const SearchData& operator+=(const SearchData& cd) { // needed for upward traversal
    moment += cd.moment;
    sum_mass += cd.sum_mass;
    centroid = moment / sum_mass;
    box.grow(cd.box);
    count += cd.count;
    return *this;
  }

  SearchData& operator=(const SearchData&) = default;

  void pup(PUP::er& p) {
    p | moment;
    p | sum_mass;
    p | centroid;
    p | box;
    p | count;
  }

};

#endif // PARATREET_SEARCH_H_
