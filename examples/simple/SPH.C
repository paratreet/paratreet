#include "Main.decl.h"
#include "Paratreet.h"
#include "SPHUtils.h"
//#include "PressureVisitor.h"
#include "DensityVisitor.h"

extern bool verify;

namespace paratreet {

  void preTraversalFn(CProxy_Driver<CentroidData>& driver, CProxy_CacheManager<CentroidData>& cache) {
    //cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    neighbor_list_collector.reset(CkCallbackResumeThread());
    part.template startUpAndDown<DensityVisitor>();
  }

  void postInteractionsFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    // by now, all density requests have gone out
    part.callPerLeafFn(CkCallbackResumeThread()); // calculates density, fills requests
    CkWaitQD();
    part.callPerLeafFn(CkCallbackResumeThread()); // calculates pressure
    if (iter == 0 && verify) {
      paratreet::outputParticles(universe, part);
    }
  }

  void perLeafFn(SpatialNode<CentroidData>& leaf) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      if (leaf.particles()[pi].density == 0) { // sum up the density. requires 0ing of densities
        CkVec<pqSmoothNode> &Q = leaf.data.neighbors[pi];
        Real density = 0.;
        auto& part = leaf.particles()[pi];
        auto& nlc_neighbors = nlc->our_neighbors[part.key];
        for (int i = 0; i < Q.size(); i++) {
          density += Q[i].mass;
          nlc_neighbors.emplace(Q[i].pKey, Q[i].pPtr);
          nlc->our_neighbors[Q[i].pKey].emplace(part.key, &part);
        }
        Q.resize(1); // we moved the memory to nlc
        auto& rsq = Q[0].fKey;
        Real r_cubed = rsq * sqrt(rsq);
        density /= (4.0 / 3.0 * M_PI * r_cubed);
        leaf.setDensity(pi, density);
        nlc->fillRequest(leaf, pi);
      }
      else {
        auto key = leaf.particles()[pi].key;
        auto nlc = neighbor_list_collector.ckLocalBranch();
        for (auto && neighbor : nlc->our_neighbors[key]) {
          if (neighbor.second == nullptr) {
            auto nbrIt = nlc->remote_particles.find(key);
            CkAssert(nbrIt == nlc->remote_particles.end());
            neighbor.second = &nbrIt->second;
          }
          doSPHCalc(leaf, pi, *neighbor.second);
        }
      }
    }
  }
}
