#ifndef SIMPLE_USERNODE_H_
#define SIMPLE_USERNODE_H_

#include "Particle.h"
#include "Node.h"

template <typename Data>
struct SourceNode {
  int depth;
  int n_particles;
  Particle* particles;
  const Data* data;

  SourceNode() : depth(0), n_particles(0), particles(nullptr), data(nullptr) {}
  SourceNode (Node<Data>* input) : depth(input->depth), n_particles(input->n_particles), particles(input->particles), data(const_cast<const Data*> (&(input->data))) {}
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


#endif // SIMPLE_NODE_H_
