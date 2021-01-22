#include "Test.decl.h"
#include "Paratreet.h"

namespace paratreet {
  void preTraversalFn(CProxy_Driver<CentroidData>& a, CProxy_CacheManager<CentroidData>& cache) {}
  void traversalFn(BoundingBox& a,CProxy_Partition<CentroidData>& b,int c) {}
  void postInteractionsFn(BoundingBox& a,CProxy_Partition<CentroidData>& b,int c) {}
  void perLeafFn(SpatialNode<CentroidData>& a) {}
}

class Main : public CBase_Main {
  paratreet::Configuration conf;

public:
  Main(CkArgMsg* m) {
    paratreet::initialize(conf, CkCallback(CkIndex_Main::run(), thisProxy));
  }

  void run() {
    CkExit();
  }
};

#include "Test.def.h"
