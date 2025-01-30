#include "BasicParticle.h"

namespace paratreet {

BasicParticle::BasicParticle() : key(Key(0)) {
  reset();
}

void BasicParticle::applyEffect(const BasicParticle::Effect& effect) {
  acceleration += effect.acceleration;
}

const BasicParticle::Effect& BasicParticle::Effect::operator+=(const BasicParticle::Effect& e) {
  acceleration += e.acceleration;
  return *this;
}

void BasicParticle::Effect::pup(PUP::er &p) {
  p | acceleration;
}

void BasicParticle::kick(Real timestep) {
  velocity += acceleration * timestep / 2;
}

void BasicParticle::perturb(Real timestep) {
  velocity += (acceleration * timestep / 2);
  acceleration = (0., 0., 0.);
  position += (velocity * timestep);
  updated_time += timestep;
}

void BasicParticle::adjustForUniverse(OrientedBox<Real> universe) {
  key = SFC::generateKey(position, universe);
  key |= (Key)1 << (KEY_BITS-1); // Add placeholder bit
}

void BasicParticle::pup(PUP::er &p) {
  p|key;
  p|order;
  p|partition_idx;
  p|mass;
  p|position;
  p|acceleration;
  p|velocity;
}

void BasicParticle::reset() {
  acceleration = Vector3D<Real>(0.0, 0.0, 0.0);
}

bool BasicParticle::operator==(const BasicParticle& other) const {
  return key == other.key;
}

bool BasicParticle::operator<=(const BasicParticle& other) const {
  return key <= other.key;
}

bool BasicParticle::operator>(const BasicParticle& other) const {
  return !(*this <= other);
}

bool BasicParticle::operator>=(const BasicParticle& other) const {
  return key >= other.key;
}

bool BasicParticle::operator<(const BasicParticle& other) const {
  return !(*this >= other);
}

} // end namespace paratreet
