#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "Node.h"
#include "Utility.h"
#include <fstream>

class TreePiece : public CBase_TreePiece {
  CkVec<Particle> particles;
  Key root;
  int n_expected;
  CkCallback callback_;
  int nPieces;
  Node *root_;
  // needed while building the local tree
  // to see which nodes are off the path from global
  // root to local root
  // populated in TreePiece::create
  Key tpRootKey_;

  public:
    TreePiece();
    void create(const CkCallback&);
    void receive(ParticleMsg*);
    void check(const CkCallback&);
    void build(const CkCallback&);
    void print(Node*);
    bool build(const CkVec<Splitter> &splitters, Node *node, bool rootLiesOnPath);
};

#endif // SIMPLE_TREEPIECE_H_
