#include "Splitter.h"
#include "Utility.h"

Splitter::Splitter() :
  from(~Key(0)), to(Key(0)), n_particles(-1)
{}

Splitter::Splitter(Key f, Key t, Key r, int n) :
  from(f), to(t), root(r), n_particles(n)
{}

void Splitter::pup(PUP::er& p){
  p | from;
  p | to;
  p | root;
  p | n_particles;
}

bool Splitter::operator<=(const Splitter& other) const {
  return from <= other.from;
}

bool Splitter::operator>=(const Splitter& other) const {
  return from >= other.from;
}

bool Splitter::operator>=(const Key& k) const {
  return from >= k;
}

Key Splitter::convertKey(Key k) {
  int depth = Utility::getDepthFromKey(k);
  return (Utility::getParticleLevelKey(k, depth));
}
