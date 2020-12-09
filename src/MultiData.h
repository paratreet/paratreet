#ifndef PARATREET_MULTIDATA_H_
#define PARATREET_MULTIDATA_H_

#include "Particle.h"
#include "Node.h"
#include "common.h"
#include "paratreet.decl.h"

#include <vector>
#include <iterator>

template <typename Data>
struct MultiData {
  int n_particles = 0;
  int n_nodes = 0;
  std::vector<Particle> particles;
  std::vector<std::pair<Key, SpatialNode<Data>>> nodes;
  int cm_index = -1;
  int tp_index = -1;

  MultiData();
  MultiData(Particle*, int, Node<Data>**, int, int, int);
  void pup(PUP::er& p);
};

template <typename Data>
MultiData<Data>::MultiData() {}

template <typename Data>
inline MultiData<Data>::MultiData(Particle* particlesi, int n_particlesi, Node<Data>** nodesi, int n_nodesi, int cm_indexi, int tp_indexi) {
  n_particles   = n_particlesi;
  n_nodes       = n_nodesi;
  cm_index      = cm_indexi;
  tp_index      = tp_indexi;
  std::copy(particlesi, particlesi + n_particles, std::back_inserter(particles));
  std::transform(nodesi, nodesi + n_nodes, std::back_inserter(nodes), [] (Node<Data>* node) {
    SpatialNode<Data> copy = *node;
    return std::make_pair(node->key, copy);
  });
}

template <typename Data>
void MultiData<Data>::pup(PUP::er& p) {
  p | n_particles;
  p | n_nodes;
  p | particles;
  p | nodes;
  p | cm_index;
  p | tp_index;
}


#endif // PARATREET_MULTIDATA_H_
