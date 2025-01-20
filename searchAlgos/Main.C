#include "Main.h"
#include "CountVisitor.h"
#include "VisitAllVisitor.h"

PARATREET_REGISTER_MAIN(ExMain);

/* readonly */ int peanoKey;
/* readonly */ CProxy_CountManager count_manager;
/* readonly */ CProxy_VisitAllTracker visit_all_tracker;

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
  peanoKey = 3;

  // Process command line arguments
  int c;

  while ((c = getopt(m->argc, m->argv, "m")) != -1) {
    switch (c) {
      case 'm':
        peanoKey = 0; // morton
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
  
  count_manager = CProxy_CountManager::ckNew(0.00001, 10000, 5);
  visit_all_tracker = CProxy_VisitAllTracker::ckNew();

  // Print configuration
  CkPrintf("\n[PARATREET]\n");
  if (conf.input_file.empty()) CkAbort("Input file unspecified");
  CkPrintf("Input file: %s\n", conf.input_file.c_str());
  CkPrintf("Decomposition type: %s\n", paratreet::asString(conf.decomp_type).c_str());
  CkPrintf("Tree type: %s\n", paratreet::asString(conf.tree_type).c_str());
  CkPrintf("Minimum number of subtrees: %d\n", conf.min_n_subtrees);
  CkPrintf("Minimum number of partitions: %d\n", conf.min_n_partitions);
  CkPrintf("Maximum number of particles per leaf: %d\n", conf.max_particles_per_leaf);

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

