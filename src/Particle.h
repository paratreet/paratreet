#ifndef PARATREET_PARTICLE_H_
#define PARATREET_PARTICLE_H_

#include "common.h"
#include "BoundingBox.h"

struct Particle {
  Key key;
  int order;
  int partition_idx;

  Real mass;
  Real density;
  Real pressure;
  Real potential;
  Vector3D<Real> position;
  Vector3D<Real> acceleration;
  Vector3D<Real> velocity;

  Particle();

  void pup(PUP::er&) ;

  void reset();

  void perturb (Real timestep, OrientedBox<Real> universe) {
    position += (velocity * timestep);
    position += (acceleration * timestep * timestep / 2);
    for (int dim = 0; dim < 3; dim++) {
      if (position[dim] < universe.lesser_corner[dim]) position[dim] += universe.greater_corner[dim] - universe.lesser_corner[dim];
      else if (position[dim] > universe.greater_corner[dim]) position[dim] -= universe.greater_corner[dim] - universe.lesser_corner[dim];
    }
    velocity += (acceleration * timestep);
    key = SFC::generateKey(position, universe);
    key |= (Key)1 << (KEY_BITS-1); // Add placeholder bit
  }

  bool operator==(const Particle&) const;
  bool operator<=(const Particle&) const;
  bool operator>(const Particle&) const;
  bool operator>=(const Particle&) const;
  bool operator<(const Particle&) const;
  friend bool operator<=(const Particle&, const Key&);
  friend bool operator>(const Particle&, const Key&);
  friend bool operator>=(const Particle&, const Key&);
  friend bool operator<(const Particle&, const Key&);
  friend bool operator<=(const Key&, const Particle&);
  friend bool operator>(const Key&, const Particle&);
  friend bool operator>=(const Key&, const Particle&);
  friend bool operator<(const Key&, const Particle&);
};

#endif // PARATREET_PARTICLE_H_
