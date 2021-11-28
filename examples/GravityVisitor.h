#ifndef PARATREET_GRAVITYVISITOR_H_
#define PARATREET_GRAVITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include <cmath>

class GravityVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;
  GravityVisitor() : offset(0, 0, 0) {}
  GravityVisitor(Vector3D<Real> offseti) : offset(offseti) {}

  void pup(PUP::er& p) {
    p | offset;
  }

private:
  Vector3D<Real> offset;

private:
  // note gconst = 1
  // note: theta defined elsewhere
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
          if (rsq != 0) {
              accel += diff * (source.particles()[j].mass / (rsq * sqrt(rsq)));
          }
      }
      target.applyAcceleration(i, accel);
    }
  }

  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.n_particles <= nMinParticleNode) return true;
    return Space::intersect(target.data.box, source.data.centroid + offset, source.data.rsq);
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
