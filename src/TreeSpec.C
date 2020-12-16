#include "TreeSpec.h"

void TreeSpec::check(const CkCallback &cb) {
  CkAssert(this->getTree() && this->getDecomposition());
  this->contribute(cb);
}

void TreeSpec::receiveDecomposition(const CkCallback& cb, Decomposition* d) {
  decomp.reset(d);
  contribute(cb);
}

Decomposition* TreeSpec::getDecomposition() {
  if (!decomp) {
    if (config.decomp_type == OCT_DECOMP) {
      decomp.reset(new OctDecomposition());
    } else if (config.decomp_type == SFC_DECOMP) {
      decomp.reset(new SfcDecomposition());
    }
  }

  return decomp.get();
}

Tree* TreeSpec::getTree() {
  if (!tree) {
    if (config.tree_type == OCT_TREE) {
      tree.reset(new OctTree());
    } else if (config.tree_type == BINARY_TREE) {
      tree.reset(new BinaryTree());
    }
  }

  return tree.get();
}

void TreeSpec::receiveConfiguration(const paratreet::Configuration& cfg, CkCallback cb) {
  setConfiguration(cfg);
  contribute(cb);
}

paratreet::Configuration& TreeSpec::getConfiguration() {
  return config;
}
