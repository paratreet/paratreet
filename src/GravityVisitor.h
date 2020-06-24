#ifndef PARATREET_GRAVITYVISITOR_H_
#define PARATREET_GRAVITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include <cmath>

extern CProxy_Resumer<CentroidData> centroid_resumer;

class GravityVisitor {
private:
  const Real gconst = 1; // Per Tom's suggestion
  const Real theta = 0.7;
  void addGravityLeaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    int curr_counter = 0;
    for (int i = 0; i < target.n_particles; i++) {
      Vector3D<Real> sum_force;
      for (int j = 0; j < source.n_particles; j++) {
        if (target.particles[i].key == source.particles[j].key) continue;
        curr_counter++;
        Vector3D<Real> diff = source.particles[j].position - target.particles[i].position;
        Real rsq = diff.lengthSquared();
        sum_force += diff * (source.particles[j].mass / (rsq * sqrt(rsq)));
      }
      target.applyForce(i, gconst * target.particles[i].mass * sum_force);
    }
#if COUNT_INTRNS
    centroid_resumer.ckLocalBranch()->countInts(curr_counter);
#endif
  }
  void addGravityNode(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      Vector3D<Real> diff = source.data.centroid - target.particles[i].position;
      Real rsq = diff.lengthSquared();
      Real scalar = (gconst * source.data.sum_mass * target.particles[i].mass / (rsq * sqrt(rsq)));
      target.applyForce(i, diff * scalar);
    }
#if COUNT_INTRNS
    centroid_resumer.ckLocalBranch()->countInts(-target.n_particles);
#endif
  }
  public:
  GravityVisitor() {}
  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    addGravityLeaf(source, target);
  }
  bool node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    Vector3D<Real> dr = source.data.centroid - target.data.centroid;
    Real dsq = dr.lengthSquared();
    if (theta * dsq < source.data.rsq) {
      return true;
    }
    if (source.data.sum_mass > 0) addGravityNode(source, target);
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
