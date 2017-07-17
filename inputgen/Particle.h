#ifndef __PARTICLE_H__
#define __PARTICLE_H__

#include "common.h"
#include "Vector3D.h"

#ifdef __CHARMC__
#include "pup.h"
#endif

struct Particle;
struct ExternalParticle {
  Vector3D<Real> position;
  Real mass;
  ExternalParticle &operator=(const Particle &p);

#ifdef __CHARMC__
  void pup(PUP::er &p){
    p|position;
    p|mass;
  }
#endif
};

struct Particle : public ExternalParticle {
  Key key;
  Vector3D<Real> velocity;
  Vector3D<Real> acceleration;
  Real potential;

#ifdef CHECK_INTER
  Real interMass;
#endif

  int order;

  Particle() : potential(0.0), acceleration(0.0) {
#ifdef CHECK_INTER
    interMass = 0.0;
#endif
  }

  bool operator<=(const Particle &other) const { return key <= other.key; }
  bool operator>=(const Particle &other) const { return key >= other.key; }
  bool operator>=(const Key &k) const {return key >= k; }

#ifdef __CHARMC__
  void pup(PUP::er &p){
    ExternalParticle::pup(p);
    p|velocity;
    p|key;
    p|potential;
  }
#endif
};


#endif
