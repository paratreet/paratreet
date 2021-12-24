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

  void ExMain::main(CkArgMsg* m) {
    // mainProxy = thisProxy;

    // Initialize readonly variables
    conf.input_file = "";
    conf.output_file = "";
    conf.min_n_subtrees = CkNumPes() * 8; // default from ChaNGa
    conf.min_n_partitions = CkNumPes() * 8;
    conf.max_particles_per_leaf = 12; // default from ChaNGa
    conf.decomp_type = paratreet::DecompType::eBinaryOct;
    conf.tree_type = paratreet::TreeType::eBinaryOct;
    conf.num_iterations = 1;
    conf.num_share_nodes = 0; // 3;
    conf.cache_share_depth= 3;
    conf.request_pause_interval = 20;
    conf.iter_pause_interval = 1000;

    peanoKey = 3;

    // Initialize member variables

    // Process command line arguments
    int c;
    std::string input_str;

    // Seek configuration files ahead of the rest of the arguments
    // (to ensure they are overriden by command-line arguments!)
    auto& n_args = m->argc;
    for (auto i = 0; i < n_args; i += 1) {
      if ((strcmp(m->argv[i], "-x") == 0) && ((i + 1) < n_args)) {
        conf.load(m->argv[i + 1]);
      }
    }

    while ((c = getopt(n_args, m->argv, "x:f:n:p:l:d:t:i:s:m")) != -1) {
      switch (c) {
        case 'x':
          break;
        case 'f':
          conf.input_file = optarg;
          break;
        case 'n':
          conf.min_n_subtrees = atoi(optarg);
          break;
        case 'p':
          conf.min_n_partitions = atoi(optarg);
          break;
        case 'l':
          conf.max_particles_per_leaf = atoi(optarg);
          break;
        case 'd':
          input_str = optarg;
          if (input_str.compare("oct") == 0) {
            conf.decomp_type = paratreet::DecompType::eOct;
          }
          else if (input_str.compare("binoct") == 0) {
            conf.decomp_type = paratreet::DecompType::eBinaryOct;
          }
          else if (input_str.compare("sfc") == 0) {
            conf.decomp_type = paratreet::DecompType::eSfc;
          }
          else if (input_str.compare("kd") == 0) {
            conf.decomp_type = paratreet::DecompType::eKd;
          }
          else if (input_str.compare("longest") == 0) {
            conf.decomp_type = paratreet::DecompType::eLongest;
          }
          break;
        case 't':
          input_str = optarg;
          if (input_str.compare("oct") == 0) {
            conf.tree_type = paratreet::TreeType::eOct;
          }
          else if (input_str.compare("binoct") == 0) {
            conf.tree_type = paratreet::TreeType::eBinaryOct;
          }
          else if (input_str.compare("kd") == 0) {
            conf.tree_type = paratreet::TreeType::eKd;
          }
          else if (input_str.compare("longest") == 0) {
            conf.tree_type = paratreet::TreeType::eLongest;
          }
          break;
        case 'i':
          conf.num_iterations = atoi(optarg);
          break;
        case 's':
          conf.num_share_nodes = atoi(optarg);
          break;
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
          CkPrintf("\t-v [filename prefix]\n");
          CkExit();
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

    count_manager = CProxy_CountManager::ckNew(0.00001, 10000, 5);
    visit_all_tracker = CProxy_VisitAllTracker::ckNew();

    // Delegate to Driver
    // CkCallback runCB(CkIndex_Main::run(), thisProxy);
    // driver = paratreet::initialize<SearchData>(conf, runCB);
  }

  void ExMain::run() {
    driver.run(CkCallbackResumeThread());

    CkExit();
  }

// #include "paratreet.def.h"
#include "templates.h"

#include "Main.def.h"

