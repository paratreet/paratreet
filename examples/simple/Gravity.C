#include "Main.decl.h"
#include "Paratreet.h"
#include "GravityVisitor.h"

extern bool verify;

namespace paratreet {

  void preTraversalFn(CProxy_Driver<CentroidData>& driver, CProxy_CacheManager<CentroidData>& cache) {
    //cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    part.template startDown<GravityVisitor>();
  }

  void postTraversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticles(universe, part);
    }
  }
  Real getTimestep(BoundingBox& universe, Real max_velocity) {
    Real universe_box_len = universe.box.greater_corner.x - universe.box.lesser_corner.x;
    return universe_box_len / max_velocity / std::cbrt(universe.n_particles);
  }

  void perLeafFn(int indicator, SpatialNode<CentroidData>& leaf) {}
}
