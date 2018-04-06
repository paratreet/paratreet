#ifndef SIMPLE_NODE_H_
#define SIMPLE_NODE_H_

#include "common.h"
#include "Particle.h"

struct Node {
  enum Type { Invalid = 0, Leaf, EmptyLeaf, RemoteLeaf, RemoteEmptyLeaf, Remote, Internal, Boundary, RemoteAboveTPKey, CachedRemote, CachedRemoteLeaf, CachedBoundary };
  
  Type type;
  Key key;
  int depth;
  Particle* particles;
  int n_particles;
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
    p | n_particles;
    if (p.isUnpacking() && n_particles)
      particles = new Particle[n_particles];
    if (n_particles) PUParray(p, particles, n_particles);
    p | n_children;
    p | owner_tp_start;
    p | owner_tp_end;
    p | wait_count;
    p | tp_index;
  }

  Node() {
    Node(-1, -1, NULL, 0, 0, 0, NULL);
  }

  Node(Key key, int depth, Particle* particles, int n_particles, int owner_tp_start, int owner_tp_end, Node* parent) {
    this->type = Invalid;
    this->key = key;
    this->depth = depth;
    this->particles = particles;
    this->n_particles = n_particles;
    this->owner_tp_start = owner_tp_start;
    this->owner_tp_end = owner_tp_end;
    this->parent = parent;
    this->n_children = 0;
    this->wait_count = -1;
    this->tp_index = -1;
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
};

#endif // SIMPLE_NODE_H_
