#include "TreeSpec.h"

void TreeSpec::receiveDecomposition(CkMarshallMsg* msg) {
  char *buffer = msg->msgBuf;
  PUP::fromMem pupper(buffer);
  PUP::detail::TemporaryObjectHolder<CkCallback> cb;
  this->getDecomposition()->pup(pupper);
  pupper | cb;
  contribute(cb.t);
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