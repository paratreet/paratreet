#ifndef PARATREET_PRESSUREVISITOR_H_
#define PARATREET_PRESSUREVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include <cmath>

struct PressureVisitor {
// in leaf check for not same particle plz
private:
  const Real radius = .01;

public:
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    Real rsq = (source.data.centroid - target.data.centroid).lengthSquared();
    return (rsq < radius * radius);
    // this just looks at the centroids when it should look at the whole boxes
    // need to use intersect(), maybe use Sphere<> ?
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        if ((target.particles()[i].position - source.particles()[j].position).lengthSquared() < radius * radius) {
          //import kernel math here
        }
      }
    }
  }
};

#endif // PARATREET_PRESSUREVISITOR_H_
