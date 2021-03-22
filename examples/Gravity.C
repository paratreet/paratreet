#include "Main.decl.h"
#include "Paratreet.h"
#include "GravityVisitor.h"

extern bool verify;
extern bool dual_tree;

namespace paratreet {

  void preTraversalFn(CProxy_Driver<CentroidData>& driver, CProxy_CacheManager<CentroidData>& cache) {
    //cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, CProxy_Subtree<CentroidData>& subtree, int iter) {
    if (dual_tree) subtree.startDual<GravityVisitor>();
    else part.template startDown<GravityVisitor>();
  }

  void postIterationFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticleAccelerations(universe, part);
    }
    if ((iter+1)%100 == 0) paratreet::outputTipsy(universe, part);
  }

  Real getFixedTimestep() {return 0.01570796326;}
  Real getTimestep(BoundingBox& universe, Real max_velocity) {
    return getFixedTimestep();
  }

  void perLeafFn(int indicator, SpatialNode<CentroidData>& leaf) {}
}
