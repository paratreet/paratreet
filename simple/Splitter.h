#ifndef SIMPLE_SPLITTER_H_
#define SIMPLE_SPLITTER_H_

#include "common.h"

struct Splitter {
  Key from;
  Key to;
  Key tp_key;
  int n_particles;

  Splitter() {}
  Splitter(Key from, Key to, Key tp_key, int n_particles) {
    this->from = from;
    this->to = to;
    this->tp_key = tp_key;
    this->n_particles = n_particles;
  }

  bool pup(PUP::er &p) {
    p | from;
    p | to;
    p | tp_key;
    p | n_particles;
  }

  bool operator<=(const Splitter& other) const {
    return from <= other.from;
  }

  bool operator>=(const Splitter& other) const {
    return from >= other.from;
  }

  bool operator<=(const Key& k) const {
    return from <= k;
  }

  bool operator>=(const Key& k) const {
    return from >= k;
  }
};

#endif // SIMPLE_SPLITTER_H_
