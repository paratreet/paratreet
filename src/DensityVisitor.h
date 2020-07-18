#ifndef PARATREET_DENSITYVISITOR_H_
#define PARATREET_DENSITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include <cmath>
#include <vector>
#include <queue>

struct DensityVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

// in leaf check for not same particle plz
private:
  const int k = 32;

public:
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (target.data.neighbors[0].size() < k) return true; // they all fill first k at the same time
    Real dsq = (source.data.centroid - target.data.centroid).lengthSquared();
    Real rsq = (target.data.neighbors[0].top().position - source.data.centroid).lengthSquared();
    // we need to look at the whole bounding box instead of just the centroid

    return (dsq < rsq);
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        target.data.neighbors[i].push(source.particles()[j]);
      }
      while (target.data.neighbors[i].size() > k) {
        target.data.neighbors[i].pop();
      }
    }
  }
};

#endif // PARATREET_DENSITYVISITOR_H_
