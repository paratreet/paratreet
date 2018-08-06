#ifndef SIMPLE_NODE_H_
#define SIMPLE_NODE_H_

#include "common.h"
#include "Particle.h"
#include <array>
#include <atomic>

template <typename Data>
struct Node {
  enum Type { Invalid = 0, Leaf, EmptyLeaf, RemoteLeaf, RemoteEmptyLeaf, Remote, Internal, Boundary, RemoteAboveTPKey, CachedRemote, CachedRemoteLeaf, CachedBoundary };
  
  Type type;
  Key key;
  int depth;
  Data data;
  int n_particles;
  Particle* particles;
  int owner_tp_start;
  int owner_tp_end;
  Node* parent;
  std::array<std::atomic<Node*>, BRANCH_FACTOR> children;
  int n_children;
  int wait_count;
  int cm_index;
  std::atomic<bool> requested;

  void pup (PUP::er& p) {
    pup_bytes(&p, (void *)&type, sizeof(Type));
    p | key;
    p | depth;
    p | data;
    p | n_particles;
    p | n_children;
    p | owner_tp_start;
    p | owner_tp_end;
    p | wait_count;
    p | cm_index;
    if (p.isUnpacking()) {
      particles = nullptr;
    }
  }

  Node() : Node(-1, -1, 0, nullptr, -1, -1, nullptr) {}

  Node(Key key, Type type, Data data, int n_children, Node* parent) :
    Node(key, parent ? parent->depth + 1 : 0, 0, nullptr, 0, 0, parent) {
    this->type = type;
    this->data = data;
    this->n_children = n_children;
  }
  Node(Key key, int depth, int n_particles, Particle* particles, int owner_tp_start, int owner_tp_end, Node* parent) {
    this->type = Invalid;
    this->key = key;
    this->depth = depth;
    this->n_particles = n_particles;
    this->particles = particles;
    this->data = Data();
    this->owner_tp_start = owner_tp_start;
    this->owner_tp_end = owner_tp_end;
    this->parent = parent;
    this->n_children = 0;
    this->wait_count = 8;
    this->cm_index = -1;
    for (int i = 0; i < BRANCH_FACTOR; i++) this->children[i].store(nullptr);
    this->requested.store(false);
  }

  Node (const Node& n) {
    type = n.type;
    key = n.key;
    depth = n.depth;
    data = n.data;
    n_particles = n.n_particles;
    particles = n.particles;
    owner_tp_start = n.owner_tp_start;
    owner_tp_end = n.owner_tp_end;
    parent = n.parent;
    n_children = n.n_children;
    wait_count = n.wait_count;
    cm_index = n.cm_index;
    for (int i = 0; i < BRANCH_FACTOR; i++) this->children[i].store(nullptr);
  }

  void triggerFree() {
    for (int i = 0; i < children.size(); i++) {
      Node* node = children[i].load();
      if (node == nullptr) continue;
      node->triggerFree();
      delete node;
    }
    if (type == CachedRemoteLeaf && n_particles) {
      delete[] particles;
    }
  }

  static std::string TypeDotColor(Type type){
    switch(type){
      case Invalid:                     return "firebrick1";
      case Internal:                    return "darkolivegreen1";
      case Leaf:                        return "darkolivegreen3";
      case EmptyLeaf:                   return "darksalmon";
      case Boundary:                    return "darkkhaki";
      case Remote:                      return "deepskyblue1";
      case RemoteLeaf:                  return "dodgerblue4";
      case RemoteEmptyLeaf:             return "deeppink";
      default:                          return "black";
    }
  }

  void dot(std::ostream& out) const {
    out << key << " [";

    out << "label=\"";
    out << key << ", ";
    out << n_particles << ", ";
    out << "[" << owner_tp_start << ", " << owner_tp_end << "]";
    //out << "\\n" << payload_;
    //out << "\\n" << tp_;
    out << "\",";

    out << "color=\"" << TypeDotColor(type) << "\", ";
    out << "style=\"filled\"";

    out << "];" << std::endl;

    if (type == Leaf || type == EmptyLeaf || type == Internal)
      return;

    if (n_children == 0) return;

    for (int i = 0; i < n_children; i++) {
      Node* child = children[i].load();
      out << key << " -> " << child->key << ";" << std::endl;
      child->dot(out);
    }
  }
  Node* findNode(Key to_find) {
    std::vector<int> remainders;
    Key temp = to_find;
    while (temp >= 8 * key) {
      remainders.push_back(temp % 8);
      temp /= 8;
    }
    Node<Data>* node = this;
    for (int i = remainders.size()-1; i >= 0; i--) {
      if (node && remainders[i] < node->children.size()) node = node->children[remainders[i]].load();
      else return nullptr;
    }
    return node;
  }
};

#endif // SIMPLE_NODE_H_
