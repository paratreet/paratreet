#ifndef SIMPLE_SPLITTER_H_
#define SIMPLE_SPLITTER_H_

#include "common.h"

struct Splitter {
  Key from;
  Key to;
  Key tp_key;
  int n_particles;

  Splitter() {}
  Splitter(Key from_, Key to_, Key tp_key_, int n_particles_) :
    from(from_), to(to_), tp_key(tp_key_), n_particles(n_particles_) {}

  void pup(PUP::er &p) {
    p | from;
    p | to;
    p | tp_key;
    p | n_particles;
  }

  bool operator<=(const Splitter& other) const {
    return from <= other.from;
  }

  bool operator>(const Splitter& other) const {
    return !(*this <= other);
  }

  bool operator>=(const Splitter& other) const {
    return from >= other.from;
  }

  bool operator<(const Splitter& other) const {
    return !(*this >= other);
  }

  bool operator<=(const Key& k) const {
    return from <= k;
  }

  bool operator>=(const Key& k) const {
    return from >= k;
  }
};

#endif // SIMPLE_SPLITTER_H_
