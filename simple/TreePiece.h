#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "Node.h"
#include "Utility.h"

#include <queue>
#include <map>
#include <stack>
#include "DataInterface.h"

template <typename Visitor, typename Data>
class TreePiece : public CBase_TreePiece<Visitor, Data> {
  std::vector<Particle> particles;
  int n_total_particles;
  int n_treepieces;
  int particle_index;
  int n_expected;
  Key tp_key; // should be a prefix of all particle keys underneath this node
  Node<Data>* root;

public:
  TreePiece(const CkCallback&, int, int); 
  void receive(ParticleMsg*);
  void check(const CkCallback&);
  void triggerRequest();
  void build(const CkCallback&);
  bool recursiveBuild(Node<Data>*, bool);
  void upOnly(CProxy_TreeElement<Visitor, Data>);
  void print(Node<Data>*);
};

/*
#define CK_TEMPLATES_ONLY
#include "simple.def.h"
#undef CK_TEMPLATES_ONLY
*/

#endif // SIMPLE_TREEPIECE_H_
