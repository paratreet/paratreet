#ifndef PARATREET_TREESPEC_H_
#define PARATREET_TREESPEC_H_

#include "paratreet.decl.h"
#include "Node.h"
#include "Modularization.h"

class TreeSpec : public CBase_TreeSpec {
public:
    TreeSpec(void)
    : subtree_decomp(nullptr),
      partition_decomp(nullptr),
      tree(nullptr) { }

    void receiveConfiguration(const CkCallback&, paratreet::Configuration*);
    void receiveDecomposition(const CkCallback&, Decomposition*, bool if_subtree);
    Decomposition* getSubtreeDecomposition();
    Decomposition* getPartitionDecomposition();
    Tree* getTree();

    void reset() {
      tree.reset();
      subtree_decomp.reset();
      partition_decomp.reset();
    }

protected:
    std::unique_ptr<Tree> tree;
    std::unique_ptr<Decomposition> subtree_decomp;
    std::unique_ptr<Decomposition> partition_decomp;

private:
  void getDecomposition(std::unique_ptr<Decomposition>& decomp, paratreet::DecompType decomp_type, bool is_subtree);
};

#endif
