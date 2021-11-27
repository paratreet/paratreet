#ifndef PARATREET_VISITALLVISITOR_H_
#define PARATREET_VISITALLVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include <cmath>
#include <vector>
#include <queue>

struct VisitAllTracker : public CBase_VisitAllTracker {
  unsigned long long sum_counts = 0u; // I dont care about overflow. just want to avoid optimizing out
  void reset(const CkCallback& cb) {
    CkPrintf("sum counts is %lld\n", sum_counts);
    sum_counts = 0u;
    this->contribute(cb);
  }
};

extern CProxy_VisitAllTracker visit_all_tracker;
 
struct VisitAllVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

  void pup(PUP::er& p) {}
public:
  static bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    auto vat = visit_all_tracker.ckLocalBranch();
    vat->sum_counts += source.n_particles;
    return true;
  }

  static void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  static void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    auto vat = visit_all_tracker.ckLocalBranch();
    vat->sum_counts += source.n_particles;
  }

private:
};

#endif // PARATREET_COLLISIONVISITOR_H_
