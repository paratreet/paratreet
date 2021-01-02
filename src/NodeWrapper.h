#ifndef _NODEWRAPPER_H_
#define _NODEWRAPPER_H_

#include "common.h"
#include "Particle.h"

struct NodeWrapper {
  Key key;
  std::vector<Particle> particles;
  int depth;

  void pup(PUP::er& p) {
    p | key;
    p | particles;
    p | depth;
  }
  NodeWrapper(Key k, std::vector<Particle>&& p, int d)
  : key(k), particles(std::move(p)), depth(d)
  {}
  NodeWrapper() = default;
};

#endif /* _NODEWRAPPER_H_ */
