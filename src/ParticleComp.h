#ifndef PARATREET_PARTICLECOMP_H_
#define PARATREET_PARTICLECOMP_H_

#include "Particle.h"

struct pqSmoothNode {
    Real fKey = 0.;// distance^2 -> place in priority queue
    Real mass = 0.;
    Key  pKey = 0ul;

    inline bool operator<(const pqSmoothNode& n) const {
      return fKey < n.fKey;
    }

    void pup(PUP::er& p) {
      p|fKey;
      p|mass;
      p|pKey;
  }
};

struct particle_comp {
  Particle p;
  particle_comp() {}
  particle_comp (const Particle& pi) : p(pi) {}
  bool operator() (const Particle& a, const Particle& b) {
    return (a.position - p.position).lengthSquared() < (b.position - p.position).lengthSquared();
  }
};

#endif // PARATREET_PARTICLECOMP_H_
