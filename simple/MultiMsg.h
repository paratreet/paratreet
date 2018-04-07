#ifndef SIMPLE_MULTIMSG_H_
#define SIMPLE_MULTIMSG_H_

#include "Particle.h"
#include "Node.h"
#include "common.h"
#include "simple.decl.h"
#include "templates.h"

template <typename Data>
struct MultiMsg : public CMessage_MultiMsg<Data> {
  Particle* particles;
  int n_particles;
  Node<Data>* nodes;
  int n_nodes;

  MultiMsg();
  MultiMsg(Particle*, int, Node<Data>*, int);
};

template <typename Data>
inline MultiMsg<Data>::MultiMsg() {
  particles = NULL;
  n_particles = 0;
  nodes = NULL;
  n_nodes = 0;
}

template <typename Data>
inline MultiMsg<Data>::MultiMsg(Particle* particlesi, int n_particlesi, Node<Data>* nodesi, int n_nodesi) {
  n_particles = n_particlesi;
  memcpy(particles, particlesi, n_particles * sizeof(Particle));
  n_nodes = n_nodesi;
  memcpy(nodes, nodesi, n_nodes * sizeof(Node<Data>)); /*
  nodes = new Node<Data> [n_nodes];
  for (int i = 0; i < n_nodes; i++) {
    nodes[i].type = nodesi[i].type;
    nodes[i].key = nodesi[i].key;
    nodes[i].depth = nodesi[i].depth;
    nodes[i].n_particles = nodesi[i].n_particles;
    nodes[i].particles = nodesi[i].particles;
    nodes[i].owner_tp_start = nodesi[i].owner_tp_start;
    nodes[i].owner_tp_end = nodesi[i].owner_tp_end;
    nodes[i].n_children = nodesi[i].n_children;
    nodes[i].tp_index = nodesi[i].tp_index;
  }*/
} 

#endif // SIMPLE_MULTIMSG_H_
