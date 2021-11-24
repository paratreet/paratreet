#include "TreeSpec.h"
#include "Paratreet.h"

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
  auto& config = paratreet::getConfiguration();
  auto decomp_type = paratreet::subtreeDecompForTree(config.tree_type);
  getDecomposition(subtree_decomp, decomp_type, true);
  return subtree_decomp.get();
}

Decomposition* TreeSpec::getPartitionDecomposition() {
  auto& config = paratreet::getConfiguration();
  getDecomposition(partition_decomp, config.decomp_type, false);
  return partition_decomp.get();
}

void TreeSpec::getDecomposition(std::unique_ptr<Decomposition>& decomp, paratreet::DecompType decomp_type, bool is_subtree) {
  if (!decomp) {
    if (decomp_type == paratreet::DecompType::eOct) {
      decomp.reset(new OctDecomposition(is_subtree));
    } else if (decomp_type == paratreet::DecompType::eBinaryOct) {
      decomp.reset(new BinaryOctDecomposition(is_subtree));
    } else if (decomp_type == paratreet::DecompType::eSfc) {
      decomp.reset(new SfcDecomposition(is_subtree));
    } else if (decomp_type == paratreet::DecompType::eKd) {
      decomp.reset(new KdDecomposition(is_subtree));
    } else if (decomp_type == paratreet::DecompType::eLongest) {
      decomp.reset(new LongestDimDecomposition(is_subtree));
    } else {
      CkAbort("dont recognize decomposition type");
    }
  }
}

Tree* TreeSpec::getTree() {
  auto& config = paratreet::getConfiguration();
  if (!tree) {
    if (config.tree_type == paratreet::TreeType::eOct) {
      tree.reset(new OctTree());
    } else if (config.tree_type == paratreet::TreeType::eBinaryOct) {
      tree.reset(new BinaryOctTree());
    } else if (config.tree_type == paratreet::TreeType::eKd) {
      tree.reset(new KdTree());
    } else if (config.tree_type == paratreet::TreeType::eLongest) {
      tree.reset(new LongestDimTree());
    } else {
      CkAbort("dont recognize tree type");
    }
  }
  return tree.get();
}
