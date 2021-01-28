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
    part.callPerLeafFn(CkCallbackResumeThread());
    part.template startDown<PressureVisitor>();
  }

  void postInteractionsFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    neighbor_list_collector.share();
    CkWaitQD();
    part.callPerLeafFn(CkCallbackResumeThread());
    if (iter == 0 && verify) {
      paratreet::outputParticles(universe, part);
    }
  }

  void perLeafFn(SpatialNode<CentroidData>& leaf) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int i = 0; i < leaf.n_particles; i++) {
      if (leaf.particles()[i].density == 0) { // sum up the density. requires 0ing of densities
        CkVec<pqSmoothNode> &Q = leaf.data.neighbors[i];
        Real density = 0.;
        for (int i = 0; i < Q.size(); i++) density += Q[i].pl.mass;
        auto& rsq = leaf.data.neighbors[i][0].fKey;
        Real rsq_cubed = rsq * rsq * rsq;
        density /= (4.0 / 3.0 * M_PI * rsq_cubed);
        leaf.setDensity(i, density);
      }
      else {
        auto key = leaf.particles()[i].key;
        auto nlc = neighbor_list_collector.ckLocalBranch();
        for (auto && neighbor : nlc->neighbors[key]) {
          leaf.applyGasWork(i, neighbor.second);
        }
      }
    }
  }
}
