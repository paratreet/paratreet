#include <algorithm>
#include <numeric>

#include "paratreet.decl.h"
#include "common.h"
#include "Reader.h"
#include "Splitter.h"
#include "TreePiece.h"
#include "TreeCanopy.h"
#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Utility.h"
#include "DensityVisitor.h"
#include "GravityVisitor.h"
#include "PressureVisitor.h"
#include "CountVisitor.h"
#include "CacheManager.h"
#include "CountManager.h"
#include "Resumer.h"
#include "Driver.h"

/* readonly */ CProxy_Main mainProxy;
/* readonly */ CProxy_Reader readers;
/* readonly */ CProxy_TreeSpec treespec;
/* readonly */ std::string input_file;
/* readonly */ int n_readers;
/* readonly */ double decomp_tolerance;
/* readonly */ int max_particles_per_tp; // For OCT decomposition
/* readonly */ int max_particles_per_leaf; // For local tree build
/* readonly */ int decomp_type;
/* readonly */ int tree_type;
/* readonly */ int num_iterations;
/* readonly */ int num_share_levels;
/* readonly */ int flush_period;
/* readonly */ CProxy_TreeCanopy<CentroidData> centroid_calculator;
/* readonly */ CProxy_CacheManager<CentroidData> centroid_cache;
/* readonly */ CProxy_Resumer<CentroidData> centroid_resumer;
/* readonly */ CProxy_CountManager count_manager;
/* readonly */ CProxy_Driver<CentroidData> centroid_driver;

class Main : public CBase_Main {
  int n_treepieces; // Cannot be a readonly because of OCT decomposition
  int cur_iteration;
  double total_start_time;
  double start_time;

  public:
  static void initialize() {
    BoundingBox::registerReducer();
  }

  Main(CkArgMsg* m) {
    mainProxy = thisProxy;

    // Initialize readonly variables
    input_file = "";
    decomp_tolerance = 0.1;
    max_particles_per_tp = 1000;
    max_particles_per_leaf = MAX_PARTICLES_PER_LEAF;
    decomp_type = OCT_DECOMP;
    tree_type = OCT_TREE;
    num_iterations = 3;
    num_share_levels = 3;
    flush_period = 1;

    // Initialize member variables
    n_treepieces = 0;
    cur_iteration = 0;

    // Process command line arguments
    int c;
    std::string input_str;
    while ((c = getopt(m->argc, m->argv, "f:n:p:l:d:t:i:s:u:")) != -1) {
      switch (c) {
        case 'f':
          input_file = optarg;
          break;
        case 'n':
          n_treepieces = atoi(optarg);
          break;
        case 'p':
          max_particles_per_tp = atoi(optarg);
          break;
        case 'l':
          max_particles_per_leaf = atoi(optarg);
          break;
        case 'd':
          input_str = optarg;
          if (input_str.compare("oct") == 0) {
            decomp_type = OCT_DECOMP;
          }
          else if (input_str.compare("sfc") == 0) {
            decomp_type = SFC_DECOMP;
          }
          break;
        case 't':
          input_str = optarg;
          if (input_str.compare("oct") == 0) {
            tree_type = OCT_TREE;
          }
          break;
        case 'i':
          num_iterations = atoi(optarg);
          break;
        case 's':
          num_share_levels = atoi(optarg);
          break;
        case 'u':
          flush_period = atoi(optarg);
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

    // FIXME: Check if runtime values match compile time values
    if (max_particles_per_leaf != MAX_PARTICLES_PER_LEAF)
      CkAbort("max_particles_per_leaf runtime value doesn't match compile time value!\n");

    // Print configuration
    CkPrintf("\n[PARATREET]\n");
    CkPrintf("Input file: %s\n", input_file.c_str());
    CkPrintf("Decomposition type: %s\n", (decomp_type == OCT_DECOMP) ? "OCT" : "SFC");
    CkPrintf("Tree type: %s\n", (tree_type == OCT_TREE) ? "OCT" : "SFC");
    if (decomp_type == SFC_DECOMP) {
      if (n_treepieces <= 0) {
        CkAbort("Number of treepieces must be larger than 0 with SFC decomposition!");
      }
      CkPrintf("Number of treepieces: %d\n", n_treepieces);
    }
    else if (decomp_type == OCT_DECOMP) {
      CkPrintf("Maximum number of particles per treepiece: %d\n", max_particles_per_tp);
    }
    CkPrintf("Maximum number of particles per leaf: %d\n\n", max_particles_per_leaf);

    // Create Readers
    n_readers = CkNumPes();
    readers = CProxy_Reader::ckNew();
    treespec = CProxy_TreeSpec::ckNew(tree_type, decomp_type);

    // Create centroid data related chares
    centroid_calculator = CProxy_TreeCanopy<CentroidData>::ckNew();
    centroid_cache = CProxy_CacheManager<CentroidData>::ckNew();
    centroid_resumer = CProxy_Resumer<CentroidData>::ckNew();
    centroid_driver = CProxy_Driver<CentroidData>::ckNew(centroid_cache, 0);
    count_manager = CProxy_CountManager::ckNew(0.00001, 10000, 5);

    // Start!
    total_start_time = CkWallTimer();
    thisProxy.run();
  }

  void run() {
    // Delegate to Driver
    centroid_driver.init(CkCallbackResumeThread());
    centroid_driver.run(CkCallbackResumeThread());

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

#include "paratreet.def.h"
