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
  int n_particles;
  int n_nodes;
  std::vector<Particle> particles;
  std::vector<std::pair<Key, SpatialNode<Data>>> nodes;

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
}


#endif // PARATREET_MULTIDATA_H_
