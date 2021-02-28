#include "Main.decl.h"
#include "Paratreet.h"
#include "CollisionVisitor.h"

extern bool verify;

namespace paratreet {

  void preTraversalFn(CProxy_Driver<CentroidData>& driver, CProxy_CacheManager<CentroidData>& cache) {
    //cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, CProxy_Subtree<CentroidData>&, int iter) {
    part.template startDown<CollisionVisitor>();
  }

  void postTraversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    // Collision is a little funky because were going to edit the mass and position of particles after a collision
    // that means were going to set the mass and position to whatever we want
    // first get minimum distance of any two particles
  }
  Real getTimestep(BoundingBox& universe, Real max_velocity) {
    Real universe_box_len = universe.box.greater_corner.x - universe.box.lesser_corner.x;
    return universe_box_len / max_velocity / std::cbrt(universe.n_particles);
  }

  void perLeafFn(int indicator, SpatialNode<CentroidData>& leaf) {}
}
