#include "TreeSpec.h"

void TreeSpec::check(const CkCallback &cb) {
  CkAssert(this->getTree() && this->getSubtreeDecomposition() && this->getPartitionDecomposition());
  this->contribute(cb);
}

void TreeSpec::receiveDecomposition(const CkCallback& cb, Decomposition* d, bool if_subtree) {
  if (if_subtree) subtree_decomp.reset(d);
  else partition_decomp.reset(d);
  contribute(cb);
}

Decomposition* TreeSpec::getSubtreeDecomposition() {
  auto decomp_type = paratreet::subtreeDecompForTree(config.tree_type);
  getDecomposition(subtree_decomp, decomp_type);
  return subtree_decomp.get();
}

Decomposition* TreeSpec::getPartitionDecomposition() {
  getDecomposition(partition_decomp, config.decomp_type);
  return partition_decomp.get();
}

void TreeSpec::getDecomposition(std::unique_ptr<Decomposition>& decomp, paratreet::DecompType decomp_type) {
  if (!decomp) {
    if (decomp_type == paratreet::DecompType::eOct) {
      decomp.reset(new OctDecomposition());
    } else if (decomp_type == paratreet::DecompType::eSfc) {
      decomp.reset(new SfcDecomposition());
    } else if (decomp_type == paratreet::DecompType::eKd) {
      decomp.reset(new KdDecomposition());
    } else if (decomp_type == paratreet::DecompType::eCoM) {
      decomp.reset(new CoMDecomposition());
    }
  }
}

Tree* TreeSpec::getTree() {
  if (!tree) {
    if (config.tree_type == paratreet::TreeType::eOct) {
      tree.reset(new OctTree());
    } else if (config.tree_type == paratreet::TreeType::eOctBinary) {
      tree.reset(new BinaryTree());
    } else if (config.tree_type == paratreet::TreeType::eKd) {
      tree.reset(new KdTree());
    } else if (config.tree_type == paratreet::TreeType::eCoM) {
      tree.reset(new CoMTree());
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
