#ifndef SIMPLE_NODE_H_
#define SIMPLE_NODE_H_

#include "common.h"
#include "Particle.h"

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
  std::vector<Node*> children;
  int n_children;
  int wait_count;
  int tp_index;

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
    p | tp_index;
  }

  Node() {
    Node(-1, -1, 0, NULL, 0, 0, NULL);
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
    this->wait_count = -1;
    this->tp_index = -1;
  }

  void triggerFree() {
    for (typename std::vector<Node*>::const_iterator it = children.begin();
         it != children.end(); ++it) {
         (*it)->triggerFree();
         delete *it;
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
      Node* child = children[i];
      out << key << " -> " << child->key << ";" << std::endl;
      child->dot(out);
    }
  }
};

#endif // SIMPLE_NODE_H_
