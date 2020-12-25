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
    CkCallback empty_cb (CkCallback::ignore);
    neighbor_list_collector.reset(empty_cb);
    part.template startUpAndDown<DensityVisitor>();
    CkWaitQD();
    part.template startDown<PressureVisitor>();
  }

  void postInteractionsFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    neighbor_list_collector.share(CkCallbackResumeThread());
    part.callPerLeafFn(CkCallbackResumeThread());
    if (iter == 0 && verify) {
      paratreet::outputParticles(universe, part);
    }
  }

  void perLeafFn(SpatialNode<CentroidData>& leaf) {
    for (int i = 0; i < leaf.n_particles; i++) {
      auto key = leaf.particles()[i].key;
      auto nlc = neighbor_list_collector.ckLocalBranch();
      auto && locals = nlc->local_neighbors[key];
      auto && remotes = nlc->remote_neighbors[key];
      std::set<Key> neighbors;
      for (auto && local : locals) {
        neighbors.insert(local.first);
        leaf.applyAcceleration(i, local.second);
      }
      for (auto && remote : remotes) {
        if (!neighbors.count(remote.first)) {
          leaf.applyAcceleration(i, remote.second);
        }
      }
    }
  }
}
