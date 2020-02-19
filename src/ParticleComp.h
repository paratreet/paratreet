#ifndef PARATREET_PARTICLECOMP_H_
#define PARATREET_PARTICLECOMP_H_

#include "Particle.h"

struct particle_comp {
  Particle p;
  particle_comp() {}
  particle_comp (Particle& pi) : p(pi) {}
  bool operator() (const Particle& a, const Particle& b) {
    return (a.position - p.position).lengthSquared() < (b.position - p.position).lengthSquared();
  }
};

#endif // PARATREET_PARTICLECOMP_H_
