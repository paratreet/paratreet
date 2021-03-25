#include "Main.decl.h"
#include "Paratreet.h"
#include "GravityVisitor.h"

extern bool verify;
extern bool dual_tree;

namespace paratreet {

  void preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (dual_tree) proxy_pack.subtree.startDual<GravityVisitor>();
    else proxy_pack.partition.template startDown<GravityVisitor>();
  }

  void postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticleAccelerations(universe, proxy_pack.partition);
    }
  }

  Real getTimestep(BoundingBox& universe, Real max_velocity) {
    Real universe_box_len = universe.box.greater_corner.x - universe.box.lesser_corner.x;
    return universe_box_len / max_velocity / std::cbrt(universe.n_particles);
  }

  void perLeafFn(int indicator, SpatialNode<CentroidData>& leaf) {}
}
