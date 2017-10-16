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
  Key splitter_key; // all local particles' keys should be greater than this
  Key first_key; // key of first local particle in SFC order
  Key last_key; // key of last local particle
  Node* root;

  public:
    TreePiece();
    void initialize(const CkCallback&);
    void receive(ParticleMsg*);
    void check(const CkCallback&);
    void build(const CkCallback&);
    void recursiveBuild(Node*, bool);
    void print(Node*);
};

#endif // SIMPLE_TREEPIECE_H_
