#include "Particle.h"

Particle::Particle() : key(Key(0)) {
  reset();
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
  potential -= pressure_dVolume * uDelta; // for adiabatic, dU = -p dV
  potential_predicted = potential - pressure_dVolume * uDelta;
  density = 0;
  pressure_dVolume = 0.;
}

void Particle::adjustNewUniverse(OrientedBox<Real> universe) {
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
  p|potential_predicted;
  p|pressure_dVolume;
  p|position;
  p|acceleration;
  p|velocity;
  p|ball;
  p|soft;
}

void Particle::reset() {
  pressure_dVolume = 0.0;
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
