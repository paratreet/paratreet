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
  p|ball;
  p|soft;
}

void Particle::reset() {
  density       = 0.0;
  pressure      = 0.0;
  potential     = 0.0;
  deltaT        = 0.01570796326; // Is there some way to make config.timestep_size accessible here?
  acceleration  = Vector3D<Real> (0.0, 0.0, 0.0);
}

void Particle::finishSetup() {
  ball          = 2.0*velocity.length()*deltaT + (4*soft);
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

bool operator<=(const Particle& p, const Key& k) {
  return p.key <= k;
}

bool operator>(const Particle& p, const Key& k) {
  return !(p <= k);
}

bool operator>=(const Particle& p, const Key& k) {
  return p.key >= k;
}

bool operator<(const Particle& p, const Key& k) {
  return !(p >= k);
}

bool operator<=(const Key& k, const Particle& p) {
  return p >= k;
}

bool operator>(const Key& k, const Particle& p) {
  return p < k;
}

bool operator>=(const Key& k, const Particle& p) {
  return p <= k;
}

bool operator<(const Key& k, const Particle& p) {
  return p > k;
}
