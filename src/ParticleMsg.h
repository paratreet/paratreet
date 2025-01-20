#ifndef PARATREET_PARTICLEMSG_H_
#define PARATREET_PARTICLEMSG_H_

#include "paratreet.decl.h"
#include "common.h"
#include "templates.h"

template <typename Data>
struct ParticleMsg : public CMessage_ParticleMsg<Data> {
  typename Data::Particle* particles;
  int n_particles;

  ParticleMsg(int n);
  ParticleMsg(typename Data::Particle* p, int n);
};

template <typename Data>
inline ParticleMsg<Data>::ParticleMsg(int n) {
  n_particles = n;
}

template <typename Data>
inline ParticleMsg<Data>::ParticleMsg(typename Data::Particle* p, int n) {
  memcpy(particles, p, n * sizeof(typename Data::Particle));
  n_particles = n;
}

#endif // PARATREET_PARTICLEMSG_H_
