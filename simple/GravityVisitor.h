#ifndef SIMPLE_GRAVITYVISITOR_H_
#define SIMPLE_GRAVITYVISITOR_H_

#include "simple.decl.h"
//#include "CentroidData.h"
#include "common.h"
#include <cmath>

extern CProxy_Driver<CentroidData> centroid_driver;
extern CProxy_CacheManager<CentroidData> centroid_cache;

class GravityVisitor {
private:
  void addGravity(const Node<CentroidData>* __restrict__ source, Node<CentroidData>* __restrict__ target, std::vector<Vector3D<Real> >& sum_forces, bool isLeaf) {
    const Real gconst = 0.1 ;// 0.000000000066742; // very small
    sum_forces.resize(target->n_particles, Vector3D<Real>(0,0,0));
    for (int i = 0; i < target->n_particles; i++) {
      //centroid_cache.ckLocalBranch()->countInt(isLeaf);
      if (isLeaf) {
        for (int j = 0; j < source->n_particles; j++) {
          if (target->particles[i].key == source->particles[j].key) continue;
	  Vector3D<Real> diff = source->particles[j].position - target->particles[i].position;
          Real rsq = diff.lengthSquared();
	  sum_forces[i] += diff * (gconst * source->particles[j].mass * target->particles[i].mass / (rsq * sqrt(rsq)));
        }
      }
      else {
        Vector3D<Real> diff = source->data.getCentroid() - target->particles[i].position;
        Real rsq = diff.lengthSquared();
        sum_forces[i] += diff * (gconst * source->data.sum_mass * target->particles[i].mass / (rsq * sqrt(rsq)));
      }
    }
  }
public:
  GravityVisitor() {}
  void leaf(const Node<CentroidData>* source, Node<CentroidData>* target) { // we're calculating force from node X on node Y
    //centroid_driver.countLeaf();
    addGravity(source, target, target->sum_forces, true);
  }
  bool node(const Node<CentroidData>* source, Node<CentroidData>* target) {
    const Real theta = 0.5;
    Vector3D<Real> dr = source->data.getCentroid() - target->data.getCentroid();
    Real dsq = dr.lengthSquared();
    if (theta * dsq < source->data.rsq) { 
      return true;
    }
    addGravity(source, target, target->sum_forces, false);
    return false;
  }
  bool cell(std::pair<Key, const CentroidData> source, std::pair<Key, const CentroidData> target) {
    // find the closest particle in target to source's center
    // check all eight corners of bounding box
    // return true if one corner is within total_volume of node
    // else return false
    const Real theta = 0.5;
    const OrientedBox<Real> box = target.second.box;
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        for (int k = 0; k < 2; k++) {
          Vector3D<Real> corner;
          corner.x = (i) ? box.greater_corner.x : box.lesser_corner.x;
          corner.y = (j) ? box.greater_corner.y : box.lesser_corner.y;
          corner.z = (k) ? box.greater_corner.z : box.lesser_corner.z;
	  Vector3D<Real> dr = source.second.getCentroid() - corner;
	  Real dsq = dr.lengthSquared();
	  if (theta * dsq < source.second.rsq) {
	    return true;
          } 
        }
      }
    }
    return false;
  }
  void pup(PUP::er& p) {}
};

#endif //SIMPLE_GRAVITYVISITOR_H_
