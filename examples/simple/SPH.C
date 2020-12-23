#include "Main.decl.h"
#include "Paratreet.h"
#include "PressureVisitor.h"
#include "DensityVisitor.h"

extern bool verify;

namespace paratreet {

  void preTraversalFn(CProxy_Driver<CentroidData>& driver, CProxy_CacheManager<CentroidData>& cache) {
    //cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    part.template startUpAndDown<DensityVisitor>();
    part.template startDown<PressureVisitor>();
  }

  void postInteractionsFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticles(universe, part);
    }
  }
}
