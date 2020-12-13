#ifndef _NODEWRAPPER_H_
#define _NODEWRAPPER_H_

#include "common.h"
#include "Particle.h"

struct NodeWrapper {
  Key key;
  int n_particles;
  int depth;

  void pup(PUP::er& p) {
    p | key;
    p | n_particles;
    p | depth;
  }
  NodeWrapper(Key k, int np, int d)
  : key(k), n_particles(np), depth(d)
  {}
  NodeWrapper() = default;
};

#endif /* _NODEWRAPPER_H_ */
