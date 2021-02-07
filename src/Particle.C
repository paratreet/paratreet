#include "Particle.h"

Particle::Particle() : key(Key(0)) {
  reset();
}

void Particle::pup(PUP::er &p) {
  p|key;
  p|order;
  p|partition_idx;
  p|mass;
  p|density;
  p|potential;
  p|potential_predicted;
  p|position;
  p|acceleration;
  p|velocity;
  p|ball;
  p|soft;
}

void Particle::reset() {
  density       = 0.0;
  acceleration  = Vector3D<Real> (0.0, 0.0, 0.0);
  deltaT        = 0.01570796326; // Is there some way to make config.timestep_size accessible here?
}

void Particle::finishInit() {
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
