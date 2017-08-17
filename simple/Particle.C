#include "Particle.h"

Particle::Particle() : key(Key(0)) {
  reset();
}

void Particle::pup(PUP::er &p) {
  p|key;
  p|order;
  p|mass;
  p|density;
  p|pressure;
  p|potential;
  p|position;
  p|acceleration;
  p|velocity;
}

void Particle::reset() {
  density = 0.0;
  pressure = 0.0;
  potential = 0.0;
  acceleration = 0.0;
  // TODO velocity?
}

bool Particle::operator<=(const Particle& other) const {
  return key <= other.key;
}

bool Particle::operator>=(const Particle& other) const {
  return key >= other.key;
}

bool Particle::operator<=(const Key& k) const {
  return key <= k;
}

bool Particle::operator>=(const Key& k) const {
  return key >= k;
}

ParticleMsg::ParticleMsg() {
  particles = NULL;
  n_particles = 0;
}

ParticleMsg::ParticleMsg(Particle* p, int n) {
  memcpy(particles, p, n * sizeof(Particle));
  n_particles = n;
}
