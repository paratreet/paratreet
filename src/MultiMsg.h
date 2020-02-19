#ifndef PARATREET_MULTIMSG_H_
#define PARATREET_MULTIMSG_H_

#include "Particle.h"
#include "Node.h"
#include "common.h"
#include "paratreet.decl.h"
#include "templates.h"

template <typename Data>
struct MultiMsg : public CMessage_MultiMsg<Data> {
  int n_particles;
  int n_nodes;
  Particle* particles;
  Node<Data>* nodes;
  MultiMsg();
  MultiMsg(Particle*, int, Node<Data>*, int);
};

template <typename Data>
inline MultiMsg<Data>::MultiMsg() {
  particles = nullptr;
  n_particles = 0;
  nodes = nullptr;
  n_nodes = 0;
}

template <typename Data>
inline MultiMsg<Data>::MultiMsg(Particle* particlesi, int n_particlesi, Node<Data>* nodesi, int n_nodesi) {
  n_particles = n_particlesi;
  memcpy(particles, particlesi, n_particles * sizeof(Particle));
  n_nodes = n_nodesi;
  memcpy(nodes, nodesi, n_nodes * sizeof(Node<Data>));
}

#endif // PARATREET_MULTIMSG_H_
