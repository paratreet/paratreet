#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "Node.h"
#include "Utility.h"

class TreePiece : public CBase_TreePiece {
  CkVec<Particle> particles;
  int cur_idx;
  int n_expected;
  Key first_key;
  Node* root;

  public:
    TreePiece();
    void initialize(const CkCallback&);
    void receive(ParticleMsg*);
    void check(const CkCallback&);
    void build(const CkCallback&);
    void recursiveBuild(Node*);
    void print(Node*);
};

#endif // SIMPLE_TREEPIECE_H_
