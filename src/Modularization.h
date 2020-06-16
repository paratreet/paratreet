#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "TreeSpec.h"
#include "Decomposition.h"
#include "common.h"

extern int decomp_type;
extern CProxy_TreeSpec treespec;

class OctTree {
public:

  static void buildCanopy(int tp_index, const SendProxyFn &fn) {
    // TODO get tp_key from decomposition
    CkAssert(decomp_type == OCT_DECOMP || decomp_type == SFC_DECOMP);
    Key tp_key = ((SfcDecomposition*)treespec.ckLocalBranch()->getDecomposition())->getTpKey(tp_index);
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
