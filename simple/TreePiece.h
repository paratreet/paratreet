#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "Node.h"

class TreePiece : public CBase_TreePiece {
  CkVec<Particle> particles;
  Key root;
  int n_expected;
  CkCallback callback_;
  int nPieces;
  Node *root_;

  public:
    TreePiece();
    void create(const CkCallback&);
    void receive(ParticleMsg*);
    void check(const CkCallback&);
    void build(const CkCallback&);
};

#endif // SIMPLE_TREEPIECE_H_
