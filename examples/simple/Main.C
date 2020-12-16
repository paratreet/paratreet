#include "Main.decl.h"
#include "Paratreet.h"
#include "GravityVisitor.h"
#include "DensityVisitor.h"
#include "PressureVisitor.h"
#include "CountVisitor.h"
#include "CollisionVisitor.h"

/* readonly */ bool verify;

namespace paratreet {
  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    part.template startDown<GravityVisitor>();
  }

  void postInteractionsFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticles(universe, part);
    }
  }
}

class Main : public CBase_Main {
  int n_treepieces; // Cannot be a readonly because of OCT decomposition
  int cur_iteration;
  double total_start_time;
  double start_time;
  paratreet::Configuration conf;

  public:
  static void initialize() {
    BoundingBox::registerReducer();
  }

  Main(CkArgMsg* m) {
    // mainProxy = thisProxy;

    // Initialize readonly variables
    conf.input_file = "";
    conf.output_file = "";
    conf.decomp_tolerance = 0.1;
    conf.max_particles_per_tp = 1000;
    conf.max_particles_per_leaf = 10;
    conf.decomp_type = OCT_DECOMP;
    conf.tree_type = OCT_TREE;
    conf.num_iterations = 3;
    conf.num_share_nodes = 0; // 3;
    conf.cache_share_depth= 3;
    conf.flush_period = 1;
    conf.timestep_size = 0.1;

    verify = false;

    // Initialize member variables
    n_treepieces = 0;
    cur_iteration = 0;

    // Process command line arguments
    int c;
    std::string input_str;
    while ((c = getopt(m->argc, m->argv, "f:n:p:l:d:t:i:s:u:v:")) != -1) {
      switch (c) {
        case 'f':
          conf.input_file = optarg;
          break;
        case 'n':
          n_treepieces = atoi(optarg);
          break;
        case 'p':
          conf.max_particles_per_tp = atoi(optarg);
          break;
        case 'l':
          conf.max_particles_per_leaf = atoi(optarg);
          break;
        case 'd':
          input_str = optarg;
          if (input_str.compare("oct") == 0) {
            conf.decomp_type = OCT_DECOMP;
          }
          else if (input_str.compare("sfc") == 0) {
            conf.decomp_type = SFC_DECOMP;
          }
          break;
        case 't':
          input_str = optarg;
          if (input_str.compare("oct") == 0) {
            conf.tree_type = OCT_TREE;
          }
          else if (input_str.compare("bin") == 0) {
            conf.tree_type = BINARY_TREE;
          }
          break;
        case 'i':
          conf.num_iterations = atoi(optarg);
          break;
        case 's':
          conf.num_share_nodes = atoi(optarg);
          break;
        case 'u':
          conf.flush_period = atoi(optarg);
          break;
        case 'v':
          verify = true;
          conf.output_file = optarg;
          if (conf.output_file.empty()) CkAbort("output file unspecified");
          break;
        default:
          CkPrintf("Usage: %s\n", m->argv[0]);
          CkPrintf("\t-f [input file]\n");
          CkPrintf("\t-n [number of treepieces]\n");
          CkPrintf("\t-p [maximum number of particles per treepiece]\n");
          CkPrintf("\t-l [maximum number of particles per leaf]\n");
          CkPrintf("\t-d [decomposition type: oct, sfc]\n");
          CkPrintf("\t-t [tree type: oct]\n");
          CkPrintf("\t-i [number of iterations]\n");
          CkPrintf("\t-s [number of shared tree levels]\n");
          CkPrintf("\t-u [flush period]\n");
          CkExit();
      }
    }
    delete m;

    // Print configuration
    CkPrintf("\n[PARATREET]\n");
    if (conf.input_file.empty()) CkAbort("Input file unspecified");
    CkPrintf("Input file: %s\n", conf.input_file.c_str());
    CkPrintf("Decomposition type: %s\n", (conf.decomp_type == OCT_DECOMP) ? "OCT" : "SFC");
    CkPrintf("Tree type: %s\n", (conf.tree_type == OCT_TREE) ? "OCT" : "BIN");
    CkPrintf("Maximum number of particles per treepiece: %d\n", conf.max_particles_per_tp);
    CkPrintf("Maximum number of particles per leaf: %d\n\n", conf.max_particles_per_leaf);

    count_manager = CProxy_CountManager::ckNew(0.00001, 10000, 5);

    // Delegate to Driver
    CkCallback runCB(CkIndex_Main::run(), thisProxy);
    paratreet::initialize(conf, runCB);
  }

  void run() {
    paratreet::run(CkCallbackResumeThread());

    CkExit();
  }

  void checkParticlesChangedDone(bool result) {
    if (result) {
      CkPrintf("[Main, iter %d] Particles are the same.\n", cur_iteration);
    }
    else {
      CkPrintf("[Main, iter %d] Particles are changed.\n", cur_iteration);
    }
  }
};
// #include "paratreet.def.h"
#include "templates.h"

#include "Main.def.h"
