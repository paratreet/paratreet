#include <algorithm>
#include <numeric>
#include "simple.decl.h"
#include "common.h"
#include "Reader.h"
#include "Splitter.h"
#include "TreePiece.h"
#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Utility.h"
#include "TreeElement.h"
#include "CentroidVisitor.h"
#include "OctTree.h"

/* readonly */ CProxy_Main mainProxy;
/* readonly */ CProxy_Reader readers;
/* readonly */ std::string input_file;
/* readonly */ int n_readers;
/* readonly */ double decomp_tolerance;
/* readonly */ int max_particles_per_tp; // for OCT decomposition
/* readonly */ int max_particles_per_leaf; // for local tree build
/* readonly */ int decomp_type;
/* readonly */ int tree_type;
/* readonly */ int num_iterations;
/* readonly */ CProxy_TreeElement<CentroidData> centroid_calculator;

class Main : public CBase_Main {
  double total_start_time;
  double start_time;
  std::string input_str;
  int cur_iteration;

  BoundingBox universe;
  const Key smallest_particle_key = Utility::removeLeadingZeros(Key(1));
  const Key largest_particle_key = (~Key(0));

  std::vector<Splitter> splitters;

  int n_treepieces;

  public:
  static void initialize() {
    BoundingBox::registerReducer();
  }

  void checkParticlesChangedDone(bool result) {
    if (result) {
      CkPrintf("[Main, iter %d] Particles are the same.\n", cur_iteration);
    } else {
      CkPrintf("[Main, iter %d] Particles are changed.\n", cur_iteration);
    }
  }

  Main(CkArgMsg* m) {
    mainProxy = thisProxy;

    // default values
    n_treepieces = 0; // cannot be a readonly because of OCT decomposition
    input_file = "";
    decomp_tolerance = 0.1;
    max_particles_per_tp = 10;
    max_particles_per_leaf = 1;
    decomp_type = OCT_DECOMP;
    tree_type = OCT_TREE;
    num_iterations = 10;
    cur_iteration = 0;

    // handle arguments
    int c;
    while ((c = getopt(m->argc, m->argv, "f:n:p:l:d:t:i:")) != -1) {
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
        default:
          CkPrintf("Usage:\n");
          CkPrintf("\t-f [input file]\n");
          CkPrintf("\t-n [number of treepieces]\n");
          CkPrintf("\t-p [maximum number of particles per treepiece]\n");
          CkPrintf("\t-l [maximum number of particles per leaf]\n");
          CkPrintf("\t-t [tree type: oct, sfc]\n");
          CkPrintf("\t-i [number of iterations]\n");
          CkExit();
      }
    }
    delete m;

    // print settings
    CkPrintf("\n[SIMPLE TREE]\n");
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

    // create Readers
    n_readers = CkNumPes();
    readers = CProxy_Reader::ckNew();
    centroid_calculator = CProxy_TreeElement<CentroidData>::ckNew();

    // start!
    total_start_time = CkWallTimer();
    thisProxy.run();
  }

  void run() {
    // load Tipsy data and build universe
    start_time = CkWallTimer();
    readers.load(input_file, CkCallbackResumeThread());
    CkPrintf("[Main] Loading Tipsy data: %lf seconds\n", CkWallTimer() - start_time);

    OctTree octtree(max_particles_per_leaf, max_particles_per_tp, 1.4, readers);

    CkReductionMsg *msg;
    for (int i = 0; i < num_iterations; i++) {
      CkPrintf("[Main, iter %d] Start redistributing particles...\n", i);

      centroid_calculator.reset();
      CkWaitQD();

      // Up traversal
      start_time = CkWallTimer();
      octtree.treepieces_proxy().upOnly<CentroidVisitor>(
        TEHolder<CentroidData>(centroid_calculator), CkCallbackResumeThread()
      );
      CkPrintf(
        "[Main, iter %d] Calculating Centroid: %lf seconds\n",
        i, CkWallTimer() - start_time
      );
      
      // Down traversal
      start_time = CkWallTimer();
      octtree.treepieces_proxy().startDown<GravityVisitor>(CkCallbackResumeThread());
      CkPrintf(
        "[Main, iter %d] Downward traversal done: %lf seconds\n",
        i, CkWallTimer() - start_time
      );

      // Purturb
      octtree.treepieces_proxy().perturb(1.0, CkCallbackResumeThread());
      CkPrintf(
        "[Main, iter %d] Perturb particles done: %lf seconds\n",
        i, CkWallTimer() - start_time
      );

      octtree.redistribute();
    }
    CkPrintf("[Main] Total time: %lf seconds\n", CkWallTimer() - total_start_time);
    CkExit();
  }

  void globalSampleSort() {
    // perform parallel sample sort of all particle keys
    int avg_n_particles = universe.n_particles / n_treepieces;
    int oversampling_ratio = 10; // std::min(10, avg_n_particles)
    if (avg_n_particles < oversampling_ratio)
      oversampling_ratio = avg_n_particles;

#ifdef DEBUG
    CkPrintf("[Main] Oversampling ratio: %d\n", oversampling_ratio);
#endif

    // gather samples
    CkReductionMsg* msg;
    readers.pickSamples(oversampling_ratio, CkCallbackResumeThread((void*&)msg));

    Key* sample_keys = (Key*)msg->getData();
    const int n_samples = msg->getSize() / sizeof(Key);

    // sort samples
    std::sort(sample_keys, sample_keys + n_samples);

    // finalize splitters
    std::vector<Key> splitter_keys(n_readers + 1);
    splitter_keys[0] = smallest_particle_key;
    for (int i = 1; i < n_readers; i++) {
      int index = (n_samples / n_readers) * i;
      splitter_keys[i] = sample_keys[index];
    }
    splitter_keys[n_readers] = largest_particle_key;

#ifdef DEBUG
    CkPrintf("[Main] Splitter keys for sorting:\n");
    for (int i = 0; i < splitter_keys.size(); i++) {
      CkPrintf("0x%" PRIx64 "\n", splitter_keys[i]);
    }
#endif

    // free reduction message
    delete msg;

    // prepare particle messages using splitters
    readers.prepMessages(splitter_keys, CkCallbackResumeThread());

    // redistribute particles to right buckets
    readers.redistribute();
    CkStartQD(CkCallbackResumeThread());

    // sort particles in local bucket
    readers.localSort(CkCallbackResumeThread());
  }
};

#include "simple.def.h"
