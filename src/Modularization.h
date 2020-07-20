#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "TreeSpec.h"
#include "Decomposition.h"
#include "common.h"

extern int decomp_type;
extern CProxy_TreeSpec treespec_subtrees;

class OctTree {
public:

  static void buildCanopy(int tp_index, const SendProxyFn &fn) {
    Key tp_key = treespec_subtrees
      .ckLocalBranch()->getDecomposition()->getTpKey(tp_index);
    Key temp_key = tp_key;
    fn(tp_key, tp_index);
    while (temp_key > 0 && temp_key % BRANCH_FACTOR == 0) {
      temp_key /= BRANCH_FACTOR;
      fn(temp_key, -1);
    }
  }

private:
  static constexpr size_t BRANCH_FACTOR = 8;
};

#endif
