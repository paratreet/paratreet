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

struct QuickSelectSFCState {
  Key start_range = Key(0);
  Key end_range = (~Key(0));
  Key compare_to() const {return start_range + (end_range - start_range) / 2;}
  int goal_rank;
  bool pending = true;
  void pup(PUP::er &p) {
    p | start_range;
    p | end_range;
    p | goal_rank;
    p | pending;
  }
};

struct QuickSelectKDState {
  int dim;
  Real start_range;
  Real end_range;
  Real compare_to() const {return start_range + (end_range - start_range) / 2;}
  int goal_rank;
  bool pending = true;
  void pup(PUP::er &p) {
    p | dim;
    p | start_range;
    p | end_range;
    p | goal_rank;
    p | pending;
  }
};



#endif // PARATREET_SPLITTER_H_
