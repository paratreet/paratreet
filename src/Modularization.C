#include "Modularization.h"
#include "TreeSpec.h"

extern CProxy_TreeSpec treespec_subtrees;

void Tree::buildCanopy(int tp_index, const SendProxyFn &fn) {
    Key tp_key = treespec_subtrees.ckLocalBranch()->getDecomposition()->getTpKey(tp_index);
    Key temp_key = tp_key;
    fn(tp_key, tp_index);
    while (temp_key > 0 && temp_key % getBranchFactor() == 0) {
        temp_key /= getBranchFactor();
        fn(temp_key, -1);
    }
}
