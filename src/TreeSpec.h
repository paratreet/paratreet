#ifndef PARATREET_TREESPEC_H_
#define PARATREET_TREESPEC_H_

#include "paratreet.decl.h"

class TreeSpec : public CBase_TreeSpec {
public:
    TreeSpec(int tree_type_, int decomp_type_)
    : tree_type(tree_type_),
      decomp_type(decomp_type_),
      decomp(nullptr) { }

    void receiveDecomposition(CkMarshallMsg*);
    Decomposition* getDecomposition();
protected:
    int decomp_type, tree_type;
    std::unique_ptr<Decomposition> decomp;
};

#endif
