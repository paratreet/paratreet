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
    if (config.decomp_type == paratreet::DecompType::eOct) {
      decomp.reset(new OctDecomposition());
    } else if (config.decomp_type == paratreet::DecompType::eSfc) {
      decomp.reset(new SfcDecomposition());
    } else if (config.decomp_type == paratreet::DecompType::eKd) {
      decomp.reset(new KdDecomposition());
    }
  }

  return decomp.get();
}

Tree* TreeSpec::getTree() {
  if (!tree) {
    if (config.tree_type == paratreet::TreeType::eOct) {
      tree.reset(new OctTree());
    } else if (config.tree_type == paratreet::TreeType::eOctBinary) {
      tree.reset(new BinaryTree());
    } else if (config.tree_type == paratreet::TreeType::eKd) {
      tree.reset(new KdTree());
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
