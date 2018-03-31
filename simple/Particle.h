#ifndef SIMPLE_PARTICLE_H_
#define SIMPLE_PARTICLE_H_

//#include "simple.decl.h"
#include "common.h"

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

  void pup(PUP::er&) ;

  void reset();

  void perturb (Real timestep, Vector3D<Real> force) {
    acceleration = force / mass;
    position = position + velocity * timestep + acceleration * timestep * timestep / 2;
    velocity = velocity + acceleration * timestep;
  }

  bool operator==(const Particle&) const;
  bool operator<=(const Particle&) const;
  bool operator>(const Particle&) const;
  bool operator>=(const Particle&) const;
  bool operator<(const Particle&) const;
  friend bool operator<=(const Particle&, const Key&);
  friend bool operator>(const Particle&, const Key&);
  friend bool operator>=(const Particle&, const Key&);
  friend bool operator<(const Particle&, const Key&);
  friend bool operator<=(const Key&, const Particle&);
  friend bool operator>(const Key&, const Particle&);
  friend bool operator>=(const Key&, const Particle&);
  friend bool operator<(const Key&, const Particle&);
};

#endif // SIMPLE_PARTICLE_H_
