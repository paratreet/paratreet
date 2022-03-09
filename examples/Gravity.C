#include "Main.h"
#include "GravityVisitor.h"

// readonly variables
extern bool verify;
extern bool dual_tree;
extern int periodic;
extern Vector3D<Real> fPeriod;
extern int nReplicas;
extern Real theta;
extern Real max_timestep;
extern int nReplicas;
extern CProxy_EwaldData ewaldProxy;

#include "EwaldData.h"
#include "Ewald.h"

  using namespace paratreet;

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
    if(periodic) {
        ewaldProxy.EwaldInit(proxy_pack.cache.ckLocalBranch()->root->data, CkCallbackResumeThread());
    }
  }

  void ExMain::traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (dual_tree && periodic) CkAbort("Not sure about this -- dual_tree and periodic both set");
    if (dual_tree) proxy_pack.subtree.startDual<GravityVisitor>(GravityVisitor(Vector3D<Real>(0, 0, 0), theta));
    if (!periodic) {
      proxy_pack.partition.template startDown<GravityVisitor>(GravityVisitor(Vector3D<Real>(0, 0, 0), theta));
    } else {
      auto replicas = [&] (int N) {
        for (int X = -N; X <= N; X++) {
          for (int Y = -N; Y <= N; Y++) {
            for (int Z = -N; Z <= N; Z++) {
              Vector3D<Real> offset (X * fPeriod.x, Y * fPeriod.y, Z * fPeriod.z);
              proxy_pack.partition.template startDown<GravityVisitor>(GravityVisitor(offset, theta));
            }
          }
        }
      };

      replicas(nReplicas); // (2*nReplicas + 1)^3 boxes
      proxy_pack.partition.callPerLeafFn(
          PARATREET_PER_LEAF_FN(LeafEwaldFn, CentroidData),
          CkCallbackResumeThread()
          );
    }
  }

  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticleAccelerations(universe, proxy_pack.partition);
    }
    else if (periodic) {
      // do ewald
    }
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    Real universe_box_len = universe.box.greater_corner.x - universe.box.lesser_corner.x;
    Real temp = universe_box_len / max_velocity / std::cbrt(universe.n_particles);
    return std::min(temp, max_timestep);
  }
