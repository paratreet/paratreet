#ifndef PARATREET_CENTROIDDATA_H_
#define PARATREET_CENTROIDDATA_H_

#include "common.h"
#include <vector>
#include <queue>
#include "Particle.h"
#include "OrientedBox.h"
#include "MultipoleMoments.h"

struct pqSmoothNode {
  Real fKey = 0.;// distance^2 -> place in priority queue
  const Particle* pPtr = nullptr;

  inline bool operator<(const pqSmoothNode& n) const {
    return fKey < n.fKey;
  }

  void pup(PUP::er& p) {
    p|fKey;
  }
};

struct CentroidData {
  MultipoleMoments multipoles;
  OrientedBox<Real> box;
  int count = 0;
  Real size_sm = 0;
  struct PerParticleStruct {
    CkVec<pqSmoothNode> neighbors; // Neighbor list for knn search
    Real ball = 0;
    Real sphBallSq = 0ull;
    Real best_dt = std::numeric_limits<Real>::max();
    const Particle* best_dt_partPtr = nullptr;
  };
  Real max_rad = 0.0;
  std::vector<PerParticleStruct> pps;

  CentroidData() = default;
  /// Construct centroid from particles.
  CentroidData(const Particle* particles, int n_particles, int depth) : CentroidData() {
    // pps is used for SPH + Collision
    Real deltaT = 0.01570796326; // Is there some way to make config.timestep_size accessible here?
    pps.resize(n_particles);

    for (int i = 0; i < n_particles; i++) {
      multipoles += particles[i];
      box.grow(particles[i].position);
      pps[i].ball = 2.0*particles[i].velocity.length()*deltaT + (4*particles[i].soft);
    }
    size_sm = 0.5*(box.size()).length();
    count = n_particles;
    if(count > 1) {
        auto tmp_box = fixSize(depth);
        // After we have a radius and centroid, we can calculate the high
        // order (scaled by radius) multipole moments.
        calculateRadiusBox(multipoles, tmp_box);
    }
    else
        multipoles.radius = 1.0; // single particle boxes don't need scaling.
  }

  /// The size of a node needs to be non-zero before calculating
  /// multipole moments.  Use a heuristic based on the size of the
  /// simulation to guess a non-zero size.
  OrientedBox<Real> fixSize(int depth) {
    auto tmp_box = box;
    if(size_sm == 0.0) { // Box has to have finite size for scaled multipole
      // calculations.
      tmp_box.grow(box.center() + size_sm);
    }
    return tmp_box;
  }

  const CentroidData& operator+=(const CentroidData& cd) { // needed for upward traversal
    box.grow(cd.box);
    multipoles += cd.multipoles;
    count += cd.count;
    size_sm = 0.5*(box.size()).length();
    if(count + cd.count > 1) {
        auto tmp_box = fixSize(0); // XXX needs fixing
        calculateRadiusFarthestCorner(multipoles, tmp_box);
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
    p | size_sm;
    p | max_rad;
    int num_leaves = pps.size();
    p | num_leaves;
    if (p.isUnpacking()) pps.resize(num_leaves);
    for (int i = 0; i < num_leaves; i++) {
      p | pps[i].ball;
    }
  }

};

#endif // PARATREET_CENTROID_H_
