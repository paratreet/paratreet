#ifndef PARATREET_NEIGHBORVISITOR_H_
#define PARATREET_NEIGHBORVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include <cmath>
#include <vector>
#include <queue>

struct NeighborVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

// in leaf check for not same particle plz
public:
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    return true;
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // Kinda hacky, but a quick way to print out the final neighbor list for each particle
    if (source.particles()[0].order != target.particles()[0].order) return;
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < target.data.neighbors[i].size(); j++) {
        CkPrintf("%d %d\n", target.particles()[i].order, target.data.neighbors[i][j].pl.order);
      }
    }
  }
};

#endif // PARATREET_NEIGHBORVISITOR_H_
