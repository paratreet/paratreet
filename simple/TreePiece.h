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
  template<typename Visitor, typename Data>
  void upOnly(CProxy_TreeElement<Visitor, Data>);
  void check(const CkCallback&);
  void triggerRequest();
  void build(const CkCallback&);
  bool recursiveBuild(Node*, bool);
  void print(Node*);
};

template<typename Visitor, typename Data>
void TreePiece::upOnly (CProxy_TreeElement<Visitor, Data> global_data) { 
  std::queue<Node*> nodes;
  nodes.push(root);
  std::map<Key, Data> local_data;
  DataInterface <Visitor, Data> d (global_data, local_data, tp_key);
  Visitor v (d);
  while (nodes.size()) {
    Node* node = nodes.front();
    nodes.pop();
    if (!local_data.count(node->key)) { // on way down
      CentroidData cd;
      local_data.insert(std::make_pair(node->key, cd));
      if (node->type == Node::Internal || node->type == Node::Boundary) {
        for (int i = 0; i < node->children.size(); i++) {
          nodes.push(node->children[i]);
        }
      }
    }
    if (node->type == Node::Leaf) {
      v.leaf(node->key, node->n_particles, node->particles);
      node->wait_count = 0;
      if (node->parent) {
        if (node->parent->wait_count < 0) node->parent->wait_count = 0;
        node->parent->wait_count++;
      }
    }
    if (node->wait_count == 0) {
      if (node->parent && v.node(local_data[node->key], node->parent->key)) { 
        if (node->parent->wait_count < 0) node->parent->wait_count = node->parent->n_children;
        node->parent->wait_count--;
        if (node->parent->wait_count == 0) nodes.push(node->parent);
      }
    }
  }
}

#endif // SIMPLE_TREEPIECE_H_
