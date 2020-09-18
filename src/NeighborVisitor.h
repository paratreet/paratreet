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
    for (int i = 0; i < target.data.neighbors.size(); i++) {
      while (target.data.neighbors[i].size() > 0) {
        int nOrder = target.data.neighbors[i].top().order;
        CkPrintf("%d %d\n", target.particles()[i].order, nOrder);
        target.data.neighbors[i].pop();
      }
    }
  }
};

#endif // PARATREET_NEIGHBORVISITOR_H_
