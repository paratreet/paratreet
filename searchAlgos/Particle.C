#include "Particle.h"

Particle::Particle() : key(Key(0)) {
  reset();
}

void Particle::applyEffect(const Particle::Effect& effect) {
  acceleration += effect.acceleration;
}

const Particle::Effect& Particle::Effect::operator+=(const Particle::Effect& e) {
  acceleration += e.acceleration;
  return *this;
}

void Particle::Effect::pup(PUP::er &p) {
  p | acceleration;
}

void Particle::kick(Real timestep) {
  velocity += acceleration * timestep / 2;
}

void Particle::perturb(Real timestep) {
  velocity += (acceleration * timestep / 2);
  acceleration = (0., 0., 0.);
  position += (velocity * timestep);
  updated_time += timestep;
}

void Particle::adjustForUniverse(OrientedBox<Real> universe) {
  key = SFC::generateKey(position, universe);
  key |= (Key)1 << (KEY_BITS-1); // Add placeholder bit
}

void Particle::pup(PUP::er &p) {
  p|key;
  p|order;
  p|partition_idx;
  p|mass;
  p|position;
  p|acceleration;
  p|velocity;
}

void Particle::reset() {
  acceleration = Vector3D<Real>(0.0, 0.0, 0.0);
}

bool Particle::operator==(const Particle& other) const {
  return key == other.key;
}

bool Particle::operator<=(const Particle& other) const {
  return key <= other.key;
}

bool Particle::operator>(const Particle& other) const {
  return !(*this <= other);
}

bool Particle::operator>=(const Particle& other) const {
  return key >= other.key;
}

bool Particle::operator<(const Particle& other) const {
  return !(*this >= other);
}
