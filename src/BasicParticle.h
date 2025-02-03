#ifndef PARATREET_BASICPARTICLE_H_
#define PARATREET_BASICPARTICLE_H_

#include "common.h"

namespace paratreet {

struct BasicParticle {
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

  BasicParticle();

  void applyEffect(const Effect& effect);

  void pup(PUP::er&);

  void reset();
  void finishInit();

  void kick(Real timestep);
  void perturb(Real timestep);
  void adjustForUniverse(OrientedBox<Real> universe);

  bool operator==(const BasicParticle&) const;
  bool operator<=(const BasicParticle&) const;
  bool operator>(const BasicParticle&) const;
  bool operator>=(const BasicParticle&) const;
  bool operator<(const BasicParticle&) const;
};

} // end namespace paratreet

#endif // PARATREET_BASICPARTICLE_H_
