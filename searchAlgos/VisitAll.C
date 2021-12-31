#include "Main.h"
#include "Paratreet.h"
#include "VisitAllVisitor.h"

  using namespace paratreet;

  void ExMain::preTraversalFn(ProxyPack<SearchData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
  }

  void ExMain::traversalFn(BoundingBox& universe, ProxyPack<SearchData>& proxy_pack, int iter) {
    proxy_pack.partition.template startDown<VisitAllVisitor>(VisitAllVisitor());
  }

  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<SearchData>& proxy_pack, int iter) {
    visit_all_tracker.reset(CkCallbackResumeThread());
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    return 0.01570796326;
  }
