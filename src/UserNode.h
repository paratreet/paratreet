#ifndef PARATREET_USERNODE_H_
#define PARATREET_USERNODE_H_

#include "Particle.h"
#include "Node.h"

template <typename Data>
struct SourceNode {
  Key key;
  int depth;
  int n_particles;
  Particle* particles;
  Data data;

  SourceNode () : key(-1), depth(0), n_particles(0), particles(nullptr) {}
  SourceNode (Node<Data>* input) : key(input->key), depth(input->depth), n_particles(input->n_particles), particles(input->particles), data(input->data) {}
};

template <typename Data>
struct TargetNode {
  int depth;
  int n_particles;
  Particle* particles;
  Data* data;
  std::vector<Vector3D<Real>>* sum_forces;

  void applyForce(int which_particle, Vector3D<Real> force) {
    if (which_particle < n_particles) {
      (*sum_forces)[which_particle] += force;
    }
  }

  TargetNode() : depth(0), n_particles(0), particles(nullptr), data(nullptr), sum_forces(nullptr) {}
  TargetNode (Node<Data>* input) : depth(input->depth), n_particles(input->n_particles), particles(input->particles), data(&(input->data)), sum_forces(&(input->sum_forces)) {}
};

#ifdef __CHARMC__
#include "pup.h"

template <typename T>
inline void operator|(PUP::er& p, SourceNode<T>& snode) {
  p | snode.key;
  p | snode.depth;
  p | snode.data;
  p | snode.n_particles;
  if (p.isUnpacking()) {
    snode.particles = nullptr;
  }
}

#endif //__CHARMC__

#endif // PARATREET_NODE_H_
