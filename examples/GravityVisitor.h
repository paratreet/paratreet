#ifndef PARATREET_GRAVITYVISITOR_H_
#define PARATREET_GRAVITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include <cmath>

class GravityVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;
  static constexpr const Real opening_geometry_factor_squared = 4.0 / 3.0;
  GravityVisitor() : offset(0, 0, 0) {}
  GravityVisitor(Vector3D<Real> offseti, Real theta) : offset(offseti), gravity_factor(opening_geometry_factor_squared / (theta * theta)) {}

  void pup(PUP::er& p) {
    p | offset;
    p | gravity_factor;
  }

private:
  Vector3D<Real> offset;
  Real gravity_factor;

private:
  // note gconst = 1
  static constexpr int  nMinParticleNode = 6;

  inline void SPLINEQ(Real invr, Real r2, Real twoh, Real& a, Real& b, Real& c, Real& d)
{
  Real u,dih,dir=(invr);
  if ((r2) < (twoh)*(twoh)) {
    dih = 2.0/(twoh);
    u = dih/dir;
    if (u < 1.0) {
      a = dih*(7.0)/5.0 
	       - 2.0/3.0*u*u 
	       + 3.0/10.0*u*u*u*u
	       - 1.0/10.0*u*u*u*u*u;
      b = dih*dih*dih*(4.0)/3.0
		       - 6.0/5.0*u*u 
		       + 1.0/2.0*u*u*u;
      c = dih*dih*dih*dih*dih*(12.0)/5.0
			       - 3.0/2.0*u;
      d = 3.0/2.0*dih*dih*dih*dih*dih*dih*dir;
    }
    else {
      a = -1.0/15.0*dir
	+ dih*(8.0/5.0)
	       - 4.0/3.0*u*u + u*u*u
	       - 3.0/10.0*u*u*u*u
	       + 1.0/30.0*u*u*u*u*u;
      b = -1.0/15.0*dir*dir*dir
	+ dih*dih*dih*(8.0)/3.0 - 3.0*u
		       + 6.0/5.0*u*u
		       - 1.0/6.0*u*u*u;
      c = -1.0/5.0*dir*dir*dir*dir*dir
	+ 3.0*dih*dih*dih*dih*dir
	+ dih*dih*dih*dih*dih*(-12.0)/5.0
			       + 1.0/2.0*u;
      d = -dir*dir*dir*dir*dir*dir*dir
	+ 3.0*dih*dih*dih*dih*dir*dir*dir
	- 1.0/2.0*dih*dih*dih*dih*dih*dih*dir;
    }
  }
  else {
    a = dir;
    b = a*a*a;
    c = 3.0*b*a*a;
    d = 5.0*c*a*a;
  }
}

inline const Real COSMO_CONST(const Real C) {return C;}
/// Calculate softened force and potential terms from cubic spline
/// density profiles.  Terms are returned in a and b.
/// 
inline
void SPLINE(Real r2, Real twoh, Real &a, Real &b)
{
  auto r = sqrt(r2);

  if (r < twoh) {
    auto dih = COSMO_CONST(2.0)/twoh;
    auto u = r*dih;
    if (u < COSMO_CONST(1.0)) {
      a = dih*(COSMO_CONST(7.0)/COSMO_CONST(5.0) 
	       - COSMO_CONST(2.0)/COSMO_CONST(3.0)*u*u 
	       + COSMO_CONST(3.0)/COSMO_CONST(10.0)*u*u*u*u
	       - COSMO_CONST(1.0)/COSMO_CONST(10.0)*u*u*u*u*u);
      b = dih*dih*dih*(COSMO_CONST(4.0)/COSMO_CONST(3.0) 
		       - COSMO_CONST(6.0)/COSMO_CONST(5.0)*u*u 
		       + COSMO_CONST(1.0)/COSMO_CONST(2.0)*u*u*u);
    }
    else {
      auto dir = COSMO_CONST(1.0)/r;
      a = COSMO_CONST(-1.0)/COSMO_CONST(15.0)*dir 
	+ dih*(COSMO_CONST(8.0)/COSMO_CONST(5.0) 
	       - COSMO_CONST(4.0)/COSMO_CONST(3.0)*u*u + u*u*u
	       - COSMO_CONST(3.0)/COSMO_CONST(10.0)*u*u*u*u 
	       + COSMO_CONST(1.0)/COSMO_CONST(30.0)*u*u*u*u*u);
      b = COSMO_CONST(-1.0)/COSMO_CONST(15.0)*dir*dir*dir 
	+ dih*dih*dih*(COSMO_CONST(8.0)/COSMO_CONST(3.0) - COSMO_CONST(3.0)*u 
		       + COSMO_CONST(6.0)/COSMO_CONST(5.0)*u*u 
		       - COSMO_CONST(1.0)/COSMO_CONST(6.0)*u*u*u);
    }
  }
  else {
    a = COSMO_CONST(1.0)/r;
    b = a*a*a;
  }
}

  inline bool openSoftening(const CentroidData& source, const CentroidData& target)
  {
    Sphere<Real> sourceSphere(source.multipoles.cm + offset, 2.0 * source.multipoles.soft);
    Sphere<Real> targetSphere(target.multipoles.cm, 2.0 * target.multipoles.soft);
    if (Space::intersect(sourceSphere, targetSphere)) return true;
    return Space::intersect(target.box, sourceSphere);
  }

  void addGravity(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      Vector3D<Real> diff = source.data.centroid + offset - target.particles()[i].position;
      Real rsq = diff.lengthSquared();
      if (rsq != 0) {
        Vector3D<Real> accel = diff * (source.data.sum_mass / (rsq * sqrt(rsq)));
        target.applyAcceleration(i, accel);
      }
    }
  }

public:
  /// @brief We've hit a leaf: N^2 interactions between all particles
  /// in the target and node.
  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      Vector3D<Real> accel(0.0);
      for (int j = 0; j < source.n_particles; j++) {
          Vector3D<Real> diff = source.particles()[j].position + offset - target.particles()[i].position;
          Real rsq = diff.lengthSquared();
          Real twoh = source.particles()[j].soft + target.particles()[i].soft;
          if (rsq != 0) {
              Real a, b;        /* potential and force terms returned
                                 * from SPLINE */
              SPLINE(rsq, twoh, a, b);
              accel += diff * b * source.particles()[j].mass;
          }
      }
      target.applyAcceleration(i, accel);
    }
  }

  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.n_particles <= nMinParticleNode) return true;
    Real dataRsq = source.data.rsq * gravity_factor;
#ifdef HEXADECAPOLE
    if(Space::intersect(target.data.box, source.data.centroid + offset, dataRsq)){
        // test for softening overlap
        if(!openSoftening(target.data, source.data)) {
            return false;       /* Passes both tests */
        }
        else {        // Open as monopole?
            // monopole criteria is much stricter
            extern Real theta;
            dataRsq *= pow(theta, -6);
            Sphere<Real> sM(source.data.centroid + offset, sqrt(dataRsq));
            return Space::intersect(target.data.box, sM);
        }
    }
#else
    return Space::intersect(target.data.box, source.data.centroid + offset, dataRsq);
#endif
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.n_particles == 0) return;
#ifdef BARNESHUT
    addGravity(source, target);
#else
    if (openSoftening(source.data, target.data)) {
      addGravity(source, target);
      return;
    }
    auto& m = source.data.multipoles;
    for (int i = 0; i < target.n_particles; i++) {
      auto& part = target.particles()[i];
      auto r = part.position - m.cm - offset;
      auto rsq = r.lengthSquared();
      Real dir = 1.0 / sqrt(rsq);
#ifdef HEXADECAPOLE
      Vector3D<Real> accel (0.0);
      Real potential = 0.0;
      Real magai;
      momEvalFmomrcm(&m.mom, m.getRadius(), dir, r.x, r.y, r.z,
		  &potential, &accel.x, &accel.y, &accel.z, &magai);
      target.applyAcceleration(i, accel);
      target.applyPotential(i, potential);
#else
      Real twoh = m.soft + part.soft;
      Real a, b, c, d; 
      SPLINEQ(dir, rsq, twoh, a, b, c, d);
      Vector3D<Real> qirv;
      qirv.x = m.xx*r.x + m.xy*r.y + m.xz*r.z;
      qirv.y = m.xy*r.x + m.yy*r.y + m.yz*r.z;
      qirv.z = m.xz*r.x + m.yz*r.y + m.zz*r.z;
      Real qir = 0.5 * dot(qirv, r);
      Real tr = 0.5 * (m.xx + m.yy + m.zz);
      Real qir3 = b*m.totalMass + d*qir - c*tr;
      target.applyPotential(i, -m.totalMass * a - c*qir + b*tr);
      auto accel = (-qir3 * r) + (c * qirv);
      target.applyAcceleration(i, accel);
#endif
    }
#endif
  }

  bool cell(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // cell means: do we want to open up target
    // for example, if source is root, we dont want to open up target
    return !Space::enclose(source.data.box, target.data.box);
  }

};

#endif //PARATREET_GRAVITYVISITOR_H_
