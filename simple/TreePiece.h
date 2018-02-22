#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "Node.h"
#include "Utility.h"

class TreePiece : public CBase_TreePiece {
  std::vector<Particle> particles;
  int n_total_particles;
  int n_treepieces;
  int particle_index;
  int n_expected;
  Key tp_key; // should be a prefix of all particle keys underneath this node
  Node* root;

  public:
    TreePiece(const CkCallback&, int, int);
    void receive(ParticleMsg*);
    template<class Data>
    void calculateData();
    void check(const CkCallback&);
    void triggerRequest();
    void build(const CkCallback&);
    bool recursiveBuild(Node*, bool);
    void print(Node*);
};

#endif // SIMPLE_TREEPIECE_H_
