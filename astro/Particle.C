#include "Particle.h"

Particle::Particle() : key(Key(0)) {
  reset();
}

Particle::Particle(const Particle& other)
: key(other.key),
  order(other.order),
  partition_idx(other.partition_idx),
  mass(other.mass),
  density(other.density),
  potential(other.potential),
  u(other.u),
  u_predicted(other.u_predicted),
  soft(other.soft),
  position(other.position),
  acceleration(other.acceleration),
  velocity(other.velocity),
  velocity_predicted(other.velocity_predicted),
  pressure_dVolume(other.pressure_dVolume),
  updated_time(other.updated_time),
  ball(other.ball),
  type(other.type)
{}

Particle& Particle::operator=(const Particle& other) {
  key = other.key;
  order = other.order;
  partition_idx = other.partition_idx;
  mass = other.mass;
  density = other.density;
  potential = other.potential;
  u = other.u;
  u_predicted = other.u_predicted;
  soft = other.soft;
  position = other.position;
  acceleration = other.acceleration;
  velocity = other.velocity;
  velocity_predicted = other.velocity_predicted;
  pressure_dVolume = other.pressure_dVolume;
  updated_time = other.updated_time;
  ball = other.ball;
  type = other.type;
  return *this;
}

void Particle::applyEffect(const Particle::Effect& effect) {
  acceleration += effect.acceleration;
  pressure_dVolume += effect.pressure;
  potential += effect.potential;
}

const Particle::Effect& Particle::Effect::operator+=(const Particle::Effect& e) {
  acceleration += e.acceleration;
  potential += e.potential;
  pressure += e.pressure;
  return *this;
}

void Particle::Effect::pup(PUP::er &p) {
  p | acceleration;
  p | potential;
  p | pressure;
}

void Particle::kick(Real timestep) {
  velocity += acceleration * timestep / 2;
}

void Particle::perturb(Real timestep) {
  velocity += (acceleration * timestep / 2);
  velocity_predicted = velocity + (acceleration * timestep);
  acceleration = (0., 0., 0.);
  position += (velocity * timestep);
  Real uDelta = 0.5e-7 * timestep;
  u -= pressure_dVolume * uDelta; // for adiabatic, dU = -p dV
  u_predicted = u - pressure_dVolume * uDelta;
  density = 0;
  pressure_dVolume = 0.;
  updated_time += timestep;
}

void Particle::adjustForUniverse(OrientedBox<Real> universe) {
  for (int dim = 0; dim < 3; dim++) {
    CkAssert(std::isfinite(position[dim]));
    while (position[dim] < universe.lesser_corner[dim]) {
      position[dim] += universe.greater_corner[dim] - universe.lesser_corner[dim];
    }
    while (position[dim] > universe.greater_corner[dim]) {
      position[dim] -= universe.greater_corner[dim] - universe.lesser_corner[dim];
    }
  }
  key = SFC::generateKey(position, universe);
  key |= (Key)1 << (KEY_BITS-1); // Add placeholder bit
}

void Particle::pup(PUP::er &p) {
  p|key;
  p|order;
  p|partition_idx;
  p|mass;
  p|density;
  p|potential;
  p|u;
  p|u_predicted;
  p|soft;
  p|position;
  p|acceleration;
  p|velocity;
  p|velocity_predicted;
  p|pressure_dVolume;
  p|updated_time;
  p|ball;
  p|type;
}

void Particle::reset() {
  pressure_dVolume = 0.0;
  density = 0.0;
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
