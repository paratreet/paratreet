#ifndef PARATREET_GRAVITYVISITOR_H_
#define PARATREET_GRAVITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include <cmath>

extern CProxy_Resumer<CentroidData> centroid_resumer;

class GravityVisitor {
private:
  // note gconst = 1
  static constexpr Real theta = 0.7;
  static constexpr int  nMinParticleNode = 6;

  void addGravity(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // Loop unrolling with a factor of 8
    const int UNROLL_FACTOR = 8;
    const Vector3D<Real>& centroid = source.data.centroid;
    const Real& sum_mass = source.data.sum_mass;
    Vector3D<Real> diff[UNROLL_FACTOR];
    Real rsq[UNROLL_FACTOR];
    Vector3D<Real> accel[UNROLL_FACTOR];

    for (int i = 0; i < target.n_particles; i += UNROLL_FACTOR) {
      diff[0] = centroid - target.particles()[i].position;
      diff[1] = centroid - target.particles()[i+1].position;
      diff[2] = centroid - target.particles()[i+2].position;
      diff[3] = centroid - target.particles()[i+3].position;
      diff[4] = centroid - target.particles()[i+4].position;
      diff[5] = centroid - target.particles()[i+5].position;
      diff[6] = centroid - target.particles()[i+6].position;
      diff[7] = centroid - target.particles()[i+7].position;
      rsq[0] = diff[0].lengthSquared();
      rsq[1] = diff[1].lengthSquared();
      rsq[2] = diff[2].lengthSquared();
      rsq[3] = diff[3].lengthSquared();
      rsq[4] = diff[4].lengthSquared();
      rsq[5] = diff[5].lengthSquared();
      rsq[6] = diff[6].lengthSquared();
      rsq[7] = diff[7].lengthSquared();
      if (rsq[0] != 0) {
        accel[0] = diff[0] * (sum_mass / (rsq[0] * sqrt(rsq[0])));
        target.applyAcceleration(i, accel[0]);
      }
      if (rsq[1] != 0) {
        accel[1] = diff[1] * (sum_mass / (rsq[1] * sqrt(rsq[1])));
        target.applyAcceleration(i+1, accel[1]);
      }
      if (rsq[2] != 0) {
        accel[2] = diff[2] * (sum_mass / (rsq[2] * sqrt(rsq[2])));
        target.applyAcceleration(i+2, accel[2]);
      }
      if (rsq[3] != 0) {
        accel[3] = diff[3] * (sum_mass / (rsq[3] * sqrt(rsq[3])));
        target.applyAcceleration(i+3, accel[3]);
      }
      if (rsq[4] != 0) {
        accel[4] = diff[4] * (sum_mass / (rsq[4] * sqrt(rsq[4])));
        target.applyAcceleration(i+4, accel[4]);
      }
      if (rsq[5] != 0) {
        accel[5] = diff[5] * (sum_mass / (rsq[5] * sqrt(rsq[5])));
        target.applyAcceleration(i+5, accel[5]);
      }
      if (rsq[6] != 0) {
        accel[6] = diff[6] * (sum_mass / (rsq[6] * sqrt(rsq[6])));
        target.applyAcceleration(i+6, accel[6]);
      }
      if (rsq[7] != 0) {
        accel[7] = diff[7] * (sum_mass / (rsq[7] * sqrt(rsq[7])));
        target.applyAcceleration(i+7, accel[7]);
      }
    }

    // Process remaining particles
    int remainder = target.n_particles % UNROLL_FACTOR;
    int k = 0;
    for (int i = target.n_particles - remainder; i < target.n_particles; i++, k++) {
      diff[k] = centroid - target.particles()[i].position;
      rsq[k] = diff[k].lengthSquared();
      if (rsq[k] != 0) {
        accel[k] = diff[k] * (sum_mass / (rsq[k] * sqrt(rsq[k])));
        target.applyAcceleration(i, accel[k]);
      }
    }
  }

  public:
  GravityVisitor() {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    addGravity(source, target);
#if COUNT_INTERACTIONS
    centroid_resumer.ckLocalBranch()->countInts(target.n_particles);
#endif
  }

  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.n_particles <= nMinParticleNode) return true;
    Vector3D<Real> dr = source.data.centroid - target.data.centroid;
    Real dsq = dr.lengthSquared();
    if (theta * dsq < source.data.rsq) {
      return true;
    }
    return false;
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.data.sum_mass > 0) {
      addGravity(source, target);
#if COUNT_INTERACTIONS
      centroid_resumer.ckLocalBranch()->countInts(-target.n_particles);
#endif
    }
  }

  bool cell(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // find the closest particle in target to source's center
    // check all eight corners of bounding box
    // return true if one corner is within total_volume of node
    // else return false

    const OrientedBox<Real> box = target.data.box;
    if (box.contains(source.data.centroid)) return true;

    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        for (int k = 0; k < 2; k++) {
          Vector3D<Real> corner;
          corner.x = (i) ? box.greater_corner.x : box.lesser_corner.x;
          corner.y = (j) ? box.greater_corner.y : box.lesser_corner.y;
          corner.z = (k) ? box.greater_corner.z : box.lesser_corner.z;
          Vector3D<Real> dr = source.data.centroid - corner;
          Real dsq = dr.lengthSquared();
          if (theta * dsq < source.data.rsq) {
            return true;
          } 
        }
      }
    }
    return false;
  }
};

#endif //PARATREET_GRAVITYVISITOR_H_
