#ifndef ASTRO_PARTICLE_H_
#define ASTRO_PARTICLE_H_

#include "common.h"

struct Particle;

struct pqSmoothNode {
  Real fKey = 0.;// distance^2 -> place in priority queue
  const Particle* pPtr = nullptr;

  inline bool operator<(const pqSmoothNode& n) const {
    return fKey < n.fKey;
  }

  void pup(PUP::er& p) {
    p|fKey;
  }
};

struct Particle {
  Key key;
  int order;
  int partition_idx = 0; // Only used when Subtree and Partition have different decomp types

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
  Real updated_time = 0.;

  std::vector<pqSmoothNode> neighbors; // Neighbor list for knn search
  Real ball = 0;
  Real sphBallSq = 0ull;
  Real best_dt = std::numeric_limits<Real>::max();
  const Particle* best_dt_partPtr = nullptr;
 
  struct Effect {
    Vector3D<Real> acceleration;
    Real potential = 0.;
    Real pressure = 0.;
    void pup(PUP::er&);
    const Effect& operator+=(const Effect& e);
  };
  Real u_predicted;

  enum class Type : char {
    eStar = 1,
    eGas  = 2,
    eDark = 3,
    eUnknown = 100
  };
  Type type = Type::eUnknown;

  Particle();
  Particle(const Particle& other);
  Particle& operator=(const Particle& other);

  bool isStar() const {return type == Type::eStar;}
  bool isGas()  const {return type == Type::eGas;}
  bool isDark() const {return type == Type::eDark;}

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
