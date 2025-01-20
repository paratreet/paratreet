#ifndef ASTRO_PARTICLE_H_
#define ASTRO_PARTICLE_H_

#include "common.h"

struct Particle {
  Key key;
  int order;
  int partition_idx = 0; // Only used when Subtree and Partition have different decomp types

  Real mass;
  Vector3D<Real> position;
  Vector3D<Real> acceleration;
  Vector3D<Real> velocity;
  Real pressure_dVolume = 0.;
  Real updated_time = 0.;
 
  struct Effect {
    Vector3D<Real> acceleration;
    void pup(PUP::er&);
    const Effect& operator+=(const Effect& e);
  };

  Particle();

  void applyEffect(const Effect& effect);

  void pup(PUP::er&);

  void reset();
  void finishInit();

  void kick(Real timestep);
  void perturb(Real timestep);
  void adjustForUniverse(OrientedBox<Real> universe);

  bool operator==(const Particle&) const;
  bool operator<=(const Particle&) const;
  bool operator>(const Particle&) const;
  bool operator>=(const Particle&) const;
  bool operator<(const Particle&) const;
};

#endif // ASTRO_PARTICLE_H_
