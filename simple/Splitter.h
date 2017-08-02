#ifndef SIMPLE_SPLITTER_H_
#define SIMPLE_SPLITTER_H_

#include "common.h"

struct Splitter {
  Key from;
  Key to;
  Key root;
  int n_particles;

  Splitter();
  Splitter(Key, Key, Key, int);

  void pup(PUP::er &p);

  bool operator<=(const Splitter &other) const;
  bool operator>=(const Splitter &other) const;
  bool operator>=(const Key &k) const;

  static Key convertKey(Key);
};

#endif // SIMPLE_SPLITTER_H_
