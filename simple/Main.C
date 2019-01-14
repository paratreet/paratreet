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
/* readonly */ std::string input_file;
/* readonly */ int n_readers;
/* readonly */ double decomp_tolerance;
/* readonly */ int max_particles_per_tp; // for OCT decomposition
/* readonly */ int max_particles_per_leaf; // for local tree build
/* readonly */ int decomp_type;
/* readonly */ int tree_type;
/* readonly */ int num_iterations;
/* readonly */ CProxy_TreeElement<CentroidData> centroid_calculator;
/* readonly */ CProxy_CacheManager<CentroidData> centroid_cache;
/* readonly */ CProxy_Resumer<CentroidData> centroid_resumer;
/* readonly */ CProxy_CountManager count_manager;
/* readonly */ CProxy_Driver<CentroidData> centroid_driver;

class Main : public CBase_Main {
  double total_start_time;
  double start_time;
  std::string input_str;
  int cur_iteration;
  int num_share_levels;
  int n_treepieces;
  int flush_period;

  public:
  static void initialize() {
    BoundingBox::registerReducer();
  }

  void terminate(CkReductionMsg* m) {
    int* bins = static_cast<int*>(m->getData());
    for (int i = 0; i < 5; i++) {
      CkPrintf("Bin[%d]: %d\n", i, bins[i]);
    }
    delete m;
    CkPrintf("[Main] Total time: %lf seconds\n", CkWallTimer() - total_start_time);
    CkExit();
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
    max_particles_per_tp = MAX_PARTICLES_PER_TP;
    max_particles_per_leaf = MAX_PARTICLES_PER_LEAF;
    decomp_type = OCT_DECOMP;
    tree_type = OCT_TREE;
    num_iterations = 100;
    cur_iteration = 0;
    num_share_levels = 3;
    flush_period = 1;

    // handle arguments
    int c;
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
    if (max_particles_per_leaf != MAX_PARTICLES_PER_LEAF)
      CkAbort("max_particles_per_leaf runtime value doesn't match compile time value!\n");
    if (max_particles_per_tp != MAX_PARTICLES_PER_TP)
      CkAbort("max particles per tp runtime value doesn't match compile time value!\n");

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
    centroid_cache = CProxy_CacheManager<CentroidData>::ckNew();
    centroid_resumer = CProxy_Resumer<CentroidData>::ckNew();
    centroid_driver = CProxy_Driver<CentroidData>::ckNew(centroid_cache, 0);
    count_manager = CProxy_CountManager::ckNew(0.00001, 10000, 5);

    // start!
    total_start_time = CkWallTimer();
    thisProxy.run();
  }

  void run() {
    Config config;
    config.input_file = input_file;
    config.tree_type = OCT_TREE;
    // ...
    centroid_driver.load(config, CkCallbackResumeThread());
    centroid_driver.run(CkCallbackResumeThread(), num_iterations);
    CkExit();
  }

#if 0
  void nextIteration() {
    CkPrintf("[Main, iter %d] Start redistributing particles...\n", cur_iteration);

    // flush data to readers
    start_time = CkWallTimer();
    treepieces.flush(readers);
    CkWaitQD(); // i think we should use callback here
    CkPrintf("[Main, iter %d] Flushing particles to Readers: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // compute universe
    start_time = CkWallTimer();
    CkReductionMsg* result;
    readers.computeUniverseBoundingBox(CkCallbackResumeThread((void*&)result));
    universe = *((BoundingBox*)result->getData());
    delete result;
    CkPrintf("[Main, iter %d] Building universe: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // assign keys
    start_time = CkWallTimer();
    readers.assignKeys(universe, CkCallbackResumeThread());
    CkPrintf("[Main, iter %d] Assigning keys and sorting particles: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // find and sort splitters
    splitters.resize(0);
    findOctSplitters();
    std::sort(splitters.begin(), splitters.end());
    readers.setSplitters(splitters, CkCallbackResumeThread());
    CkPrintf("[Main, iter %d] Finding and sorting splitters: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // reset TreeElement
    centroid_calculator.reset();
    CkWaitQD();

    // flush data to treepieces
    start_time = CkWallTimer();
    readers.flush(universe.n_particles, n_treepieces, treepieces);
    CkWaitQD();
    CkPrintf("[Main, iter %d] Flushing particles to TreePieces: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // rebuild local tree in TreePieces
    start_time = CkWallTimer();
    treepieces.rebuild(true);
    CkPrintf("[Main, iter %d] Local tree rebuild: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // debug
    CkCallback cb(CkReductionTarget(Main, checkParticlesChangedDone), thisProxy);
    treepieces.checkParticlesChanged(cb);
    CkWaitQD();

    // start traversals
    start_time = CkWallTimer();
  }

  void findOctSplitters() {
    BufferedVec<Key> keys;

    // initial splitter keys (first and last)
    keys.add(Key(1)); // 0000...1
    keys.add(~Key(0)); // 1111...1
    keys.buffer();


    int decomp_particle_sum = 0; // to check if all particles are decomposed

    // main decomposition loop
    while (keys.size() != 0) {
      // send splitters to Readers for histogramming
      CkReductionMsg *msg;
      readers.countOct(keys.get(), CkCallbackResumeThread((void*&)msg));
      int* counts = (int*)msg->getData();
      int n_counts = msg->getSize() / sizeof(int);

      // check counts and create splitters if necessary
      Real threshold = (DECOMP_TOLERANCE * Real(max_particles_per_tp));
      for (int i = 0; i < n_counts; i++) {
        Key from = keys.get(2*i);
        Key to = keys.get(2*i+1);

        int n_particles = counts[i];
        if ((Real)n_particles > threshold) {
          // create 8 more splitter key pairs to go one level deeper
          // leading zeros will be removed in Reader::count()
          // to compare splitter key with particle keys
          keys.add(from << 3);
          keys.add((from << 3) + 1);

          keys.add((from << 3) + 1);
          keys.add((from << 3) + 2);

          keys.add((from << 3) + 2);
          keys.add((from << 3) + 3);

          keys.add((from << 3) + 3);
          keys.add((from << 3) + 4);

          keys.add((from << 3) + 4);
          keys.add((from << 3) + 5);

          keys.add((from << 3) + 5);
          keys.add((from << 3) + 6);

          keys.add((from << 3) + 6);
          keys.add((from << 3) + 7);

          keys.add((from << 3) + 7);
          if (to == (~Key(0)))
            keys.add(~Key(0));
          else
            keys.add(to << 3);
        }
        else {
          // create and store splitter
          Splitter sp(Utility::removeLeadingZeros(from),
              Utility::removeLeadingZeros(to), from, n_particles);
          splitters.push_back(sp);

          // add up number of particles to check if all are flushed
          decomp_particle_sum += n_particles;
        }
      }

      keys.buffer();
      delete msg;
    }

    if (decomp_particle_sum != universe.n_particles) {
      CkPrintf("[Main] ERROR! Only %d particles out of %d decomposed\n",
          decomp_particle_sum, universe.n_particles);
      CkAbort("Decomposition error");
    }

    // determine number of TreePieces
    // override input from user if there was one
    n_treepieces = splitters.size();
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
#endif
};

#include "simple.def.h"
