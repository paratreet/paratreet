#ifndef SIMPLE_NODEMSG_H_
#define SIMPLE_NODEMSG_H_

#include "Node.h"
#include "common.h"
#include "simple.decl.h"
#include "templates.h"

template <typename Data>
struct NodeMsg : public CMessage_NodeMsg<Data> {
  Node<Data>* nodes;
  int n_nodes;
//hi
  NodeMsg();
  NodeMsg(Node<Data>*, int);
};

template<typename Data>
inline NodeMsg<Data>::NodeMsg() {
  nodes = NULL;
  n_nodes = 0;
}

template<typename Data>
inline NodeMsg<Data>::NodeMsg(Node<Data>* nodesi, int n_nodesi) {
  n_nodes = n_nodesi;
  nodes = new Node<Data> [n_nodes];
  for (int i = 0; i < n_nodes; i++) {
    nodes[i].type = nodesi[i].type;
    nodes[i].key = nodesi[i].key ;
    nodes[i].depth = nodesi[i].depth ;
    nodes[i].n_particles = nodesi[i].n_particles ;
    nodes[i].particles = nodesi[i].particles;
    nodes[i].owner_tp_start = nodesi[i].owner_tp_start ;
    nodes[i].owner_tp_end = nodesi[i].owner_tp_end ;
    nodes[i].n_children = nodesi[i].n_children ;
    nodes[i].tp_index = nodesi[i].tp_index ;
  }
}

#endif // SIMPLE_NODEMSG_H_
