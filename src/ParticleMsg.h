#ifndef PARATREET_PARTICLEMSG_H_
#define PARATREET_PARTICLEMSG_H_

#include "Particle.h"
#include "common.h"
#include "paratreet.decl.h"

struct ParticleMsg : public CMessage_ParticleMsg {
  Particle* particles;
  int n_particles;

  ParticleMsg();
  ParticleMsg(Particle* p, int n);
};

inline ParticleMsg::ParticleMsg() {
  particles = nullptr;
  n_particles = 0;
}

inline ParticleMsg::ParticleMsg(Particle* p, int n) {
  memcpy(particles, p, n * sizeof(Particle));
  n_particles = n;
}

#endif // PARATREET_PARTICLEMSG_H_
