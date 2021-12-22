#ifndef PARATREET_PARTICLE_H_
#define PARATREET_PARTICLE_H_

#include "common.h"
#include "BoundingBox.h"

struct Particle {
  Key key;
  int order;
  int partition_idx = 0; // Only used when Subtree and Patition have different decomp types

  Real mass;
  Real density;
  Real potential;
  Real u;
  Real soft;
  Vector3D<Real> position;
  Vector3D<Real> acceleration;
  Vector3D<Real> velocity;
  Vector3D<Real> velocity_predicted;
  Real pressure_dVolume = 0.;
  Real u_predicted;

  enum class Type : char {
    eStar = 1,
    eGas  = 2,
    eDark = 3,
    eUnknown = 100
  };
  Type type = Type::eUnknown;

  Particle();

  bool isStar() const {return type == Type::eStar;}
  bool isGas()  const {return type == Type::eGas;}
  bool isDark() const {return type == Type::eDark;}

  void pup(PUP::er&) ;

  void reset();
  void finishInit();

  void kick(Real timestep);
  void perturb(Real timestep);
  void adjustNewUniverse(OrientedBox<Real> universe);

  bool operator==(const Particle&) const;
  bool operator<=(const Particle&) const;
  bool operator>(const Particle&) const;
  bool operator>=(const Particle&) const;
  bool operator<(const Particle&) const;
};

#endif // PARATREET_PARTICLE_H_
