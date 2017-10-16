#ifndef SIMPLE_NODE_H_
#define SIMPLE_NODE_H_

#include "common.h"

typedef struct Node {
  enum Type { Leaf, EmptyLeaf, RemoteLeaf, RemoteEmptyLeaf, Remote, Internal, Boundary };

  Type type;
  Key key;
  Key start, end;
  int depth;
  int n_particles;
  int n_children;
  std::vector<Node*> children;
  Node* parent;

  Node() {}

  Node(Key key, Key start, Key end, int depth, int n_particles, Node* parent) {
    this->key = key;
    this->start = start;
    this->end = end;
    this->depth = depth;
    this->n_particles = n_particles;
    this->parent = parent;
  }
} Node;

#endif // SIMPLE_NODE_H_
