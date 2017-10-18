#ifndef SIMPLE_PARTICLE_H_
#define SIMPLE_PARTICLE_H_

#include "simple.decl.h"
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

  void pup(PUP::er&);

  void reset();

  bool operator<=(const Particle&) const;
  bool operator>=(const Particle&) const;
  bool operator<=(const Key&) const;
  bool operator>=(const Key&) const;
};

struct ParticleMsg : public CMessage_ParticleMsg {
  Particle* particles;
  int n_particles;

  ParticleMsg();
  ParticleMsg(Particle* p, int n);
};

#endif // SIMPLE_PARTICLE_H_
