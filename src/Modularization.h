#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "paratreet.decl.h"
#include "Node.h"
#include "Utility.h"

class Tree {
public:
  virtual int getBranchFactor() = 0;
  virtual void buildCanopy(int tp_index, const SendProxyFn &fn) = 0;
};

class OctTree : public Tree {
public:
  int getBranchFactor() override {
    return 8;
  }

  void buildCanopy(int tp_index, const SendProxyFn &fn) override;

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
};

class BinaryTree : public OctTree {
public:
  int getBranchFactor() override {
    return 2;
  }
};

#endif
