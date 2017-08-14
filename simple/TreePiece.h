#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"

class TreePiece : public CBase_TreePiece {
  CkVec<Particle> particles;
  Key root;
  int n_expected;

  public:
    TreePiece();
    void receive(int n, Particle* p);
};

#endif // SIMPLE_TREEPIECE_H_
