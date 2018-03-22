#ifndef SIMPLE_NODE_H_
#define SIMPLE_NODE_H_

#include "common.h"

template <typename Data>
struct Node {
  enum Type { Invalid = 0, Leaf, EmptyLeaf, RemoteLeaf, RemoteEmptyLeaf, Remote, Internal, Boundary };

  Type type;
  Key key;
  int depth;
  Particle* particles;
  Data* data;
  int n_particles;
  int owner_tp_start;
  int owner_tp_end;
  Node* parent;
  std::vector<Node*> children;
  int n_children;
  int wait_count;

  Node() {}

  Node(Key key, int depth, Particle* particles, int n_particles, int owner_tp_start, int owner_tp_end, Node* parent) {
    this->type = Invalid;
    this->key = key;
    this->depth = depth;
    this->particles = particles;
    this->data = NULL;
    this->n_particles = n_particles;
    this->owner_tp_start = owner_tp_start;
    this->owner_tp_end = owner_tp_end;
    this->parent = parent;
    this->n_children = 0;
    this->wait_count = -1;
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
      Node* child = children[i];
      out << key << " -> " << child->key << ";" << std::endl;
      child->dot(out);
    }
  }
};

#endif // SIMPLE_NODE_H_
