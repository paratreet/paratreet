#include "Particle.h"

struct particle_comp {
  Particle p;
  particle_comp() {}
  particle_comp (Particle& pi) : p(pi) {}
  Real distsq(Vector3D<Real> p1, Vector3D<Real> p2) {
    Real dsq = (p1.x - p2.x) * (p1.x - p2.x);
    dsq += (p1.y - p2.y) * (p1.y - p2.y);
    dsq += (p1.z - p2.z) * (p1.z - p2.z);
    return dsq;
  }
  bool operator() (Particle a, Particle b) {
    return distsq(a.position, p.position) > distsq(b.position, p.position);
  }
};
