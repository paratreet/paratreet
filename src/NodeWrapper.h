#ifndef _NODEWRAPPER_H_
#define _NODEWRAPPER_H_

#include "common.h"

template <typename Data>
struct NodeWrapper {
  Key key;
  int n_particles;
  int depth;
  int branch_factor;
  bool is_leaf;
  Data data;

  void pup(PUP::er& p) {
    p | key;
    p | depth;
    p | data;
    p | is_leaf;
    p | branch_factor;
  }

  NodeWrapper(Key k, int np, int de, int bf, bool l, Data d)
    : key(k), n_particles(np), depth(de), branch_factor(bf), is_leaf(l), data(d)
    {}

  NodeWrapper() {}
};

#endif /* _NODEWRAPPER_H_ */
