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
  int n_expected;
  Key root_key;
  Node *root_node;
  CkCallback callback_;

  public:
    TreePiece();
    void initialize(const CkCallback&);
    void receive(ParticleMsg*);
    void check(const CkCallback&);
    void build(const CkCallback&);
    void print(Node*);
    bool build(Node*, bool);
};

#endif // SIMPLE_TREEPIECE_H_
