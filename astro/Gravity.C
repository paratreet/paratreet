#include "Main.h"
#include "GravityVisitor.h"

// readonly variables
extern AstroFields astroConf;
extern CProxy_EwaldData ewaldProxy;

#include "EwaldData.h"
#include "Ewald.h"

  using namespace paratreet;

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
    if(astroConf.periodic) {
        ewaldProxy.EwaldInit(proxy_pack.cache.ckLocalBranch()->root->data, CkCallbackResumeThread());
    }
  }

  void ExMain::traversalFn(const BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (astroConf.dual_tree && astroConf.periodic) CkAbort("Not sure about this -- dual_tree and periodic both set");
    if (astroConf.dual_tree) proxy_pack.subtree.startDual<GravityVisitor>(GravityVisitor(Vector3D<Real>(0, 0, 0), astroConf.theta));
    if (!astroConf.periodic) {
      proxy_pack.partition.template startDown<GravityVisitor>(GravityVisitor(Vector3D<Real>(0, 0, 0), astroConf.theta));
    } else {
      auto replicas = [&] (int N) {
        for (int X = -N; X <= N; X++) {
          for (int Y = -N; Y <= N; Y++) {
            for (int Z = -N; Z <= N; Z++) {
              Vector3D<Real> offset (X * astroConf.fPeriod.x, Y * astroConf.fPeriod.y, Z * astroConf.fPeriod.z);
              proxy_pack.partition.template startDown<GravityVisitor>(GravityVisitor(offset, astroConf.theta));
            }
          }
        }
      };

      replicas(astroConf.nReplicas); // (2*nReplicas + 1)^3 boxes
      proxy_pack.partition.callPerLeafFn(
          PARATREET_PER_LEAF_FN(LeafEwaldFn, CentroidData),
          CkCallbackResumeThread()
          );
    }
  }

  void ExMain::postIterationFn(const BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (iter == 0 && !astroConf.output_file.empty()) {
      paratreet::outputSorted(astroConf.output_file, universe, proxy_pack, iter, std::vector<int>{0, 1, 2});
    }
  }

  Real ExMain::getTimestep(const BoundingBox& universe, Real max_velocity) {
    Real universe_box_len = universe.box.greater_corner.x - universe.box.lesser_corner.x;
    Real temp = universe_box_len / max_velocity / std::cbrt(universe.n_particles);
    return std::min(temp, astroConf.max_timestep);
  }
