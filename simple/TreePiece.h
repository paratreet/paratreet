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
  int n_treepieces;
  Key tp_key; // should be a prefix of all particle keys underneath this node
  Node* root;

  public:
    TreePiece(const CkCallback&);
    //void initialize(const CkCallback&);
    void receive(ParticleMsg*);
    void check(const CkCallback&);
    void build(const CkCallback&);
    bool recursiveBuild(Node*, bool);
    void print(Node*);
};

#endif // SIMPLE_TREEPIECE_H_
