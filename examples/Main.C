#include "Main.h"

#include "GravityVisitor.h"
#include "DensityVisitor.h"
#include "PressureVisitor.h"
#include "CollisionVisitor.h"
#include "EwaldData.h"
#include "unionFindLib.h"

PARATREET_REGISTER_MAIN(ExMain);

/* readonly */ bool verify;
/* readonly */ bool dual_tree;
/* readonly */ int periodic;
/* readonly */ int peanoKey;
/* readonly */ Vector3D<Real> fPeriod;
/* readonly */ int nReplicas;
/* readonly */ Real theta;
/* readonly */ int iter_start_collision;
/* readonly */ Real max_timestep;
/* readonly */ CProxy_EwaldData ewaldProxy;

// only included here so code will compile properly without FoF, not used
/* readonly */ CProxy_UnionFindLib libProxy;

  static void initialize() {
    BoundingBox::registerReducer();
  }

  void ExMain::setDefaults(void) {
    conf.min_n_subtrees = CkNumPes() * 8; // default from ChaNGa
    conf.min_n_partitions = CkNumPes() * 8;
    conf.max_particles_per_leaf = 12; // default from ChaNGa
    conf.decomp_type = paratreet::DecompType::eBinaryOct;
    conf.tree_type = paratreet::TreeType::eBinaryOct;
    conf.num_iterations = 3;
    conf.num_share_nodes = 0; // 3;
    conf.cache_share_depth = 3;
    conf.pool_elem_size; 
    conf.flush_period = 0;
    conf.flush_max_avg_ratio = 10.;
    conf.lb_period = 5;
    conf.request_pause_interval = 20;
    conf.iter_pause_interval = 100;
  }

  void ExMain::main(CkArgMsg* m) {
    // Initialize readonly variables
    verify = !conf.output_file.empty();
    dual_tree = false;
    periodic = false;
    fPeriod = std::numeric_limits<float>::max();
    nReplicas = 0;
    conf.periodic = periodic;
    conf.fPeriod = fPeriod;
    conf.nReplicas = nReplicas;

    peanoKey = 3;
    theta = 0.7;
    iter_start_collision = 0;
    max_timestep = 1e-5;

    
    // Process command line arguments
    int c;
    std::string input_str;

    while ((c = getopt(m->argc, m->argv, "mec:j:")) != -1) {
      switch (c) {
        case 'm':
          peanoKey = 0; // morton
          break;
        case 'e':
          dual_tree = true;
          CkPrintf("You are doing a dual-tree traversal. Make sure you have matching decomps.\n");
          break;
        case 'c':
          iter_start_collision = atoi(optarg);
          break;
        case 'j':
          max_timestep = atof(optarg);
          break;

        default:
          CkPrintf("Usage: %s\n", m->argv[0]);
          CkPrintf("\t-f [input file]\n");
          CkPrintf("\t-n [number of treepieces]\n");
          CkPrintf("\t-p [maximum number of particles per treepiece]\n");
          CkPrintf("\t-l [maximum number of particles per leaf]\n");
          CkPrintf("\t-d [decomposition type: oct, sfc, kd]\n");
          CkPrintf("\t-t [tree type: oct, bin, kd]\n");
          CkPrintf("\t-i [number of iterations]\n");
          CkPrintf("\t-s [number of shared tree levels]\n");
          CkPrintf("\t-u [flush period]\n");
          CkPrintf("\t-r [flush threshold for Subtree max_average ratio]\n");
          CkPrintf("\t-b [load balancing period]\n");
          CkPrintf("\t-v [filename prefix]\n");
          CkPrintf("\t-j [max timestep]\n");
      }
    }
    delete m;

    // Print configuration
    CkPrintf("\n[PARATREET]\n");
    if (conf.input_file.empty()) CkAbort("Input file unspecified");
    CkPrintf("Input file: %s\n", conf.input_file.c_str());
    CkPrintf("Decomposition type: %s\n", paratreet::asString(conf.decomp_type).c_str());
    CkPrintf("Tree type: %s\n", paratreet::asString(conf.tree_type).c_str());
    CkPrintf("Minimum number of subtrees: %d\n", conf.min_n_subtrees);
    CkPrintf("Minimum number of partitions: %d\n", conf.min_n_partitions);
    CkPrintf("Maximum number of particles per leaf: %d\n", conf.max_particles_per_leaf);
    CkPrintf("Max timestep: %lf\n\n", max_timestep);

    periodic = conf.periodic;
    fPeriod = conf.fPeriod;
    nReplicas = conf.nReplicas;

    ewaldProxy = CProxy_EwaldData::ckNew();

    // Delegate to Driver
    // CkCallback runCB(CkIndex_Main::run(), thisProxy);
    // driver = paratreet::initialize<CentroidData>(conf, runCB);
  }

  void ExMain::run() {
    driver.run(CkCallbackResumeThread());

    CkExit();
  }

// #include "paratreet.def.h"
#include "templates.h"

#include "Main.def.h"

