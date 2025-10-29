#ifndef PARATREET_FOFVISITOR_H
#define PARATREET_FOFVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include "CentroidData.h"
#include <cmath>
#include <vector>
#include <queue>
#include "unionFindLib.h"
#include "Partition.h"

extern CProxy_UnionFindLib libProxy;
extern CProxy_Partition<CentroidData> partitionProxy;
extern Real linkingLength;
extern Vector3D<Real> fPeriod;

class FoFVisitor {

private:
  Vector3D<Real> offset;
public:
  static constexpr const bool CallSelfLeaf = true;
  FoFVisitor() : offset(0, 0, 0) {}
  FoFVisitor(Vector3D<Real> offseti) : offset(offseti) {}


  void pup(PUP::er& p) {
    p | offset;
  }


  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    Real r_bucket = target.data.size_sm + linkingLength;
    if (!Space::intersect(source.data.box, target.data.box.center()+offset, r_bucket*r_bucket))
      return false;

    // Check if any of the target balls intersect the source volume
    for (int i = 0; i < target.n_particles; i++) {
      Real ballSq = linkingLength * linkingLength;
      //adding offset to target needs to be the same in leaf
      if(Space::intersect(source.data.box, target.particles()[i].position+offset, ballSq))
        return true;
    }
    return false;
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        const Particle& sp = source.particles()[j];
        const Particle& tp = target.particles()[i];
        Real distance = (tp.position - sp.position + offset).length();
        // union two particles if source and target particles linking length spheres intersect
        // avoid union of same pair twice by comapring particle order (the particle ID) with "<" operator
        if (distance < linkingLength && sp.order < tp.order) {
          libProxy[tp.partition_idx].ckLocal()->union_request(sp.vertex_id, tp.vertex_id);
        }
      }
    }
  }
};

#endif // PARATREET_FOFVISITOR_H_
