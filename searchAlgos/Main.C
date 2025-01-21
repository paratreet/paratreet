#include "Main.h"
#include "CountVisitor.h"
#include "VisitAllVisitor.h"

PARATREET_REGISTER_MAIN(ExMain);

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
  conf.peanoKey = 3;
}

void ExMain::main(CkArgMsg* m) {
  // Initialize readonly variables
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

