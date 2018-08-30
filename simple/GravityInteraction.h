#ifndef SIMPLE_GRAVITY_INTERACTION_H_
#define SIMPLE_GRAVITY_INTERACTION_H_
#include "Particle.h"
#include "Vector3D.h"

void invokeKernel(Particle* from_particles, int from_n_particles,
    const Vector3D<Real>& from_centroid, Real from_sum_mass, Particle* on_particles,
    int on_n_particles, Vector3D<Real>* sum_forces, Real gconst, bool is_leaf);

#endif
