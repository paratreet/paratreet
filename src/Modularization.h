#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "TreeSpec.h"
#include "Decomposition.h"
#include "common.h"

extern CProxy_TreeSpec treespec;

class OctTree {
public:

  static void buildCanopy(int tp_index, const SendProxyFn &fn) {
    Key tp_key = treespec.ckLocalBranch()->getDecomposition()->getTpKey(tp_index);
    Key temp_key = tp_key;
    fn(tp_key, tp_index);
    while (temp_key > 0 && temp_key % BRANCH_FACTOR == 0) {
      temp_key /= BRANCH_FACTOR;
      fn(temp_key, -1);
    }
  }

  // Returns start + n_particles
  template<typename Data>
  static int findChildsLastParticle(Node<Data>* parent, int child, Key child_key, int start, int finish) {
    Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1);

    // Find number of particles in child
    if (child < parent->n_children - 1) {
      return Utility::binarySearchGE(sibling_splitter, parent->particles(), start, finish);
    } else {
      return finish;
    }
  }

private:
  static constexpr size_t BRANCH_FACTOR = 8;
};

#endif
