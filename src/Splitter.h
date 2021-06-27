#ifndef PARATREET_SPLITTER_H_
#define PARATREET_SPLITTER_H_

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
};

struct GenericSplitter {
  Key 	start_key = Key(0);
  Key 	end_key = (~Key(0));
  Key 	midKey() const {return start_key + (end_key - start_key) / 2;}
  Key 	split_key = Key(0);
  int 	goal_rank;
  bool 	pending = true;
  int 	dim = -1;
  Real  start_float = 0;
  Real  end_float   = 0;
  Real  midFloat() const {return start_float + (end_float - start_float) / 2;}
  void pup(PUP::er &p) {
    p | start_key;
    p | end_key;
    p | split_key;
    p | goal_rank;
    p | pending;
    p | dim;
    p | start_float;
    p | end_float;
  }
};

#endif // PARATREET_SPLITTER_H_
