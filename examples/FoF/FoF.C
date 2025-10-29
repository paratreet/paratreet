#include <iostream>
#include "Paratreet.h"
#include "CentroidData.h"
#include "unionFindLib.h"
#include "FoFVisitor.h"
#include "FoF.decl.h"

/* readonly */ bool outputFileConfigured;
/* readonly */ CProxy_UnionFindLib libProxy;
/* readonly */ CProxy_Partition<CentroidData> partitionProxy;
/* readonly */ int peanoKey;
/* readonly */ Real linkingLength;
/* readonly */ int minVerticesPerComponent; // minimum is strictly greater than this value
int periodic;
Vector3D<Real> fPeriod;
bool verify;

using namespace paratreet;

static void initialize() {
  BoundingBox::registerReducer();
}

class FoF : public paratreet::Main<CentroidData> {
  void main(CkArgMsg* m) override {
    // Initialize readonly variables
    if (conf.input_file.empty()) 
      CkPrintf("warning: no input file provided\n");
    CkAssert(!conf.input_file.empty());

    peanoKey = 3;
    linkingLength = conf.linking_length;
    fPeriod=conf.fPeriod;
    periodic = conf.periodic;

    minVerticesPerComponent = conf.min_vertices_per_component;

    // Process command line arguments
    int c;
    std::string input_str;

    while ((c = getopt(m->argc, m->argv, "mec:j:")) != -1) {
      switch (c) {
        case 'm':
          peanoKey = 0; // morton space filling curves
          break;
        
        default:
          CkPrintf("Usage: %s\n", m->argv[0]);
          CkPrintf("\t-n [minimum number of treepieces]\n");
          CkPrintf("\t-p [minimum number of partitions]\n");
          CkPrintf("\t-l [maximum number of particles per leaf]\n");
          CkPrintf("\t-d [decomposition type: oct, sfc, kd]\n");
          CkPrintf("\t-t [tree type: oct, bin, kd]\n");
          CkPrintf("\t-i [number of iterations]\n");
          CkPrintf("\t-s [number of shared tree levels]\n");
          CkPrintf("\t-u [flush period]\n");
          CkPrintf("\t-r [flush threshold for Subtree max_average ratio]\n");
          CkPrintf("\t-b [load balancing period]\n");

          CkPrintf("\t-e [set a gravatational softening for all particles]\n");
          CkPrintf("\t-f [input file]\n");
          CkPrintf("\t-v [output file prefix]\n");

          CkPrintf("\t-ll [linking length]\n");
          CkPrintf("\t-pbc [periodic boundary conditions]\n");
          CkPrintf("\t-px (-py, -pz) [x period (y period, z period)]\n");
          CkPrintf("\t-c [minimum vertices per component]\n");
          CkExit();
      }
    }
    delete m;

    // Print configuration
    CkPrintf("\n[PARATREET]\n");
    if (conf.input_file.empty()) CkAbort("Input file unspecified");
    CkPrintf("Input file: %s\n", conf.input_file.c_str());
    CkPrintf("Output file prefix: %s\n", conf.output_file.empty() ? "output file prefix not provided" : conf.output_file.c_str());
    verify = conf.output_file.empty() ? false : true;
    CkPrintf("Decomposition type: %s\n", paratreet::asString(conf.decomp_type).c_str());
    CkPrintf("Tree type: %s\n", paratreet::asString(conf.tree_type).c_str());
    CkPrintf("Minimum number of subtrees: %d\n", conf.min_n_subtrees);
    CkPrintf("Minimum number of partitions: %d\n", conf.min_n_partitions);
    CkPrintf("Maximum number of particles per leaf: %d\n", conf.max_particles_per_leaf);
    CkPrintf("Linking length for friends-of-friends: %f\n", conf.linking_length);
    CkPrintf("Minimum vertices per group for friends-of-friends is strictly greater than: %d\n", minVerticesPerComponent);
    
    // main::initializeDriver() will be run after main exits
    // After that main::run() is ran. See Paratreet.C::MainChare class
  }


  void setDefaults(void) {
    conf.min_n_subtrees = CkNumPes() * 8; // default from ChaNGa
    conf.min_n_partitions = CkNumPes() * 8;
    conf.max_particles_per_leaf = 12; // default from ChaNGa
    conf.decomp_type = paratreet::DecompType::eBinaryOct;
    conf.tree_type = paratreet::TreeType::eBinaryOct;
    conf.num_iterations = 1;
    conf.num_share_nodes = 0; // 3;
    conf.cache_share_depth = 3;
    conf.pool_elem_size;
    conf.flush_period = 0;
    conf.flush_max_avg_ratio = 10.;
    conf.lb_period = 5;
    conf.request_pause_interval = 20;
    conf.iter_pause_interval = 1000;
    conf.min_vertices_per_component = 8; // default from ChaNGa
    conf.linking_length = 0.2; // default from ChaNGa
  }

  // -------------------
  // Traversal functions
  // -------------------
  void preTraversalFn(ProxyPack<CentroidData>& proxy_pack) override {
    // The size of the starter pack of data loaded by the cache manager is
    // specified in Configuration.cache_share_depth
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
    
    // Store proxies as global variables for access
    libProxy = proxy_pack.libProxy;
    partitionProxy = proxy_pack.partition;
  }

  void traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) override {
    
    //only need to look at cubes that are almost touching (N=1)
    if(!periodic)
    {
      proxy_pack.partition.template startDown<FoFVisitor>(FoFVisitor(Vector3D<Real> (0,0,0)));
    }
    else
    {
    for (int X = -1; X <= 1; ++X) {
        for (int Y = -1; Y <= 1; ++Y) {
          for (int Z = -1; Z <= 1; ++Z) {
            Vector3D<Real> offset (X * fPeriod.x, Y * fPeriod.y, Z * fPeriod.z);
            //Vector3D<Real> offset (X, Y, Z);
            proxy_pack.partition.template startDown<FoFVisitor>(FoFVisitor(offset));
          }
        }
      }
    }
  }

  void postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) override {
    CkPrintf("[Main] Inverted trees constructed for unionFindLib. Performing components detection\n");
    int startTime = CkWallTimer();
    libProxy.find_components(CkCallbackResumeThread());

    CkPrintf("[Main] Components identified. Prune components with too few particles now\n");
    CkPrintf("[Main] Components detection time: %f\n", CkWallTimer() - startTime);
    startTime = CkWallTimer();
    // min vertices per component is strictly greater than minVerticesPerComponent
    libProxy.prune_components(minVerticesPerComponent, CkCallbackResumeThread());

    CkPrintf("[Main] Components pruned. Labeling particles with component numbers\n");
    CkPrintf("[Main] Component pruning time: %f\n", CkWallTimer() - startTime);
    startTime = CkWallTimer();
    partitionProxy.getConnectedComponents(CkCallbackResumeThread());
  
    CkPrintf("[Main] Components pruned and labeled. Outputting results of friends-of-friends\n");
    CkPrintf("[Main] Component labeling time: %f\n", CkWallTimer() - startTime);
    startTime = CkWallTimer();
    if(verify) paratreet::outputParticleAccelerations(universe, partitionProxy);

    CkPrintf("[Main] Output complete for friends-of-friends\n");
    CkPrintf("[Main] Writing to output time: %f\n", CkWallTimer() - startTime);
  }
  
  Real getTimestep(BoundingBox& universe, Real max_velocity) override {
    return 0;
  }

  void run() override {
    driver.run(CkCallbackResumeThread());
    CkExit();
  }
};

PARATREET_REGISTER_MAIN(FoF);
#include "templates.h"
#include "FoF.def.h"
