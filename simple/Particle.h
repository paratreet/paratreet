#ifndef SIMPLE_PARTICLE_H_
#define SIMPLE_PARTICLE_H_

#include "common.h"
#include "Vector3D.h"

struct Particle {
  Key key;
  int order;

  Real mass;
  Real density;
  Real pressure;
  Real potential;
  Vector3D<Real> position;
  Vector3D<Real> acceleration;
  Vector3D<Real> velocity;

  Particle();

  void pup(PUP::er &P);

  void reset();

  bool operator<=(const Particle&) const;
  bool operator>=(const Particle&) const;
  bool operator<=(const Key&) const;
  bool operator>=(const Key&) const;
};

#endif // SIMPLE_PARTICLE_H_
