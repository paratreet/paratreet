#ifndef PARATREET_PARTICLECOMP_H_
#define PARATREET_PARTICLECOMP_H_

#include "Particle.h"

struct pqSmoothNode {
    double fKey;    // distance^2 -> place in priority queue
    Vector3D<double> dx; // displacement of this particle
    //Particle pl; // particle data
    void pup(PUP::er& p) {
      p|fKey;
      p|dx;
      //p|pl;
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
