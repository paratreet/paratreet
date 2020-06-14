#ifndef PARATREET_MULTIDATA_H_
#define PARATREET_MULTIDATA_H_

#include "Particle.h"
#include "Node.h"
#include "common.h"
#include "paratreet.decl.h"

template <typename Data>
struct MultiData {
  static constexpr size_t MAX_BRANCH_FACTOR = 8;
  static constexpr size_t MAX_NUM_PARTICLES = MAX_BRANCH_FACTOR * MAX_BRANCH_FACTOR * MAX_PARTICLES_PER_LEAF;
  static constexpr size_t MAX_NUM_NODES     = 1 + MAX_BRANCH_FACTOR + MAX_BRANCH_FACTOR * MAX_BRANCH_FACTOR;
  int n_particles;
  int n_nodes;
  Particle particles [MAX_NUM_PARTICLES];
  std::pair<Key, SpatialNode<Data>> nodes [MAX_NUM_NODES];

  MultiData();
  MultiData(Particle*, int, Node<Data>**, int);
  void pup(PUP::er& p);
};

template <typename Data>
inline MultiData<Data>::MultiData()
  : n_particles(0),
    n_nodes(0)
{
}

template <typename Data>
inline MultiData<Data>::MultiData(Particle* particlesi, int n_particlesi, Node<Data>** nodesi, int n_nodesi) {
  n_particles   = n_particlesi;
  n_nodes       = n_nodesi;
  std::copy(particlesi, particlesi + n_particles, particles);
  std::transform(nodesi, nodesi + n_nodes, nodes, [] (Node<Data>* node) {
    SpatialNode<Data> copy = *node;
    return std::make_pair(node->key, copy);
  });
}

template <typename Data>
void MultiData<Data>::pup(PUP::er& p) {
  p | n_particles;
  p | n_nodes;
  PUParray(p, particles, MAX_NUM_PARTICLES); // TODO varTram
  PUParray(p, nodes,     MAX_NUM_NODES);
}


#endif // PARATREET_MULTIDATA_H_
