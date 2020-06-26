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
    int curr_counter = 0;
    for (int i = 0; i < target.n_particles; i++) {
      Vector3D<Real> diff = source.data.centroid - target.particles()[i].position;
      Real rsq = diff.lengthSquared();
      if (rsq != 0) {
        Vector3D<Real> accel = diff * (source.data.sum_mass / (rsq * sqrt(rsq)));
        target.applyAcceleration(i, accel);
      }
    }
  }
  public:
  GravityVisitor() {}
  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    addGravity(source, target);
#if COUNT_INTRNS
    centroid_resumer.ckLocalBranch()->countInts(target.n_particles);
#endif
  }
  bool node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (source.n_particles <= nMinParticleNode) return true;
    Vector3D<Real> dr = source.data.centroid - target.data.centroid;
    Real dsq = dr.lengthSquared();
    if (theta * dsq < source.data.rsq) {
      return true;
    }
    if (source.data.sum_mass > 0) {
      addGravity(source, target);
#if COUNT_INTRNS
      centroid_resumer.ckLocalBranch()->countInts(-target.n_particles);
#endif
    }
    return false;
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
