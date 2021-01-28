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
  Real ball;
  Real deltaT;
  Real soft;
  Vector3D<Real> position;
  Vector3D<Real> acceleration;
  Vector3D<Real> velocity;
  Vector3D<Real> velocity_predicted;
  Real pressure_dVolume;
  Real potential_predicted;

  Particle();

  void pup(PUP::er&) ;

  void reset();
  void finishInit();

  void perturb (Real timestep, OrientedBox<Real> universe) {
    position += (velocity * timestep);
    position += (acceleration * timestep * timestep / 2);
    for (int dim = 0; dim < 3; dim++) {
      while (position[dim] < universe.lesser_corner[dim]) {
        position[dim] += universe.greater_corner[dim] - universe.lesser_corner[dim];
      }
      while (position[dim] > universe.greater_corner[dim]) {
        position[dim] -= universe.greater_corner[dim] - universe.lesser_corner[dim];
      }
    }
    velocity += (acceleration * timestep);
    velocity_predicted = velocity + (acceleration * timestep);
    Real uDelta = 0.5e-7 * timestep;
    potential -= pressure_dVolume * uDelta; // for adiabatic, dU = -p dV
    potential_predicted = potential - pressure_dVolume * uDelta;
    key = SFC::generateKey(position, universe);
    key |= (Key)1 << (KEY_BITS-1); // Add placeholder bit
    density = 0;
  }

  bool operator==(const Particle&) const;
  bool operator<=(const Particle&) const;
  bool operator>(const Particle&) const;
  bool operator>=(const Particle&) const;
  bool operator<(const Particle&) const;
};

#endif // PARATREET_PARTICLE_H_
