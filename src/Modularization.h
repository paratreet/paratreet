#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "Decomposition.h"

class OctTree {
public:

  static void buildCanopy(const std::vector<Splitter> &splitters, int tp_index, const SendProxyFn &fn) {
    Key tp_key = splitters[tp_index].tp_key;
    Key temp_key = tp_key;
    fn(tp_key, tp_index);
    while (temp_key > 0 && temp_key % BRANCH_FACTOR == 0) {
      temp_key /= BRANCH_FACTOR;
      fn(temp_key, -1);
    }
  }
};

#endif
