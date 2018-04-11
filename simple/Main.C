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
  Key smallest_particle_key;
  Key largest_particle_key;

  std::vector<Splitter> splitters;

  CProxy_TreePiece<CentroidData> treepieces; // cannot be a global variable
  int n_treepieces;

  public:
  static void initialize() {
    BoundingBox::registerReducer();
  }
  
  void doneUp() {
    CkPrintf("[Main, iter %d] Calculating Centroid: %lf seconds\n", cur_iteration, CkWallTimer() - start_time);
    start_time = CkWallTimer();
    treepieces.startDown<GravityVisitor>();
  }

  void doneDown() {
    CkPrintf("[Main, iter %d] Downward traversal done: %lf seconds\n", cur_iteration, CkWallTimer() - start_time);
    if (++cur_iteration < num_iterations)
      nextIteration();
    else
      terminate();
  }

  void terminate() {
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
    max_particles_per_tp = 10;
    max_particles_per_leaf = 1;
    decomp_type = OCT_DECOMP;
    tree_type = OCT_TREE;
    num_iterations = 1;
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
    // useful particle keys
    smallest_particle_key = Utility::removeLeadingZeros(Key(1));
    largest_particle_key = (~Key(0));

    // load Tipsy data and build universe
    start_time = CkWallTimer();
    CkReductionMsg* result;
    readers.load(input_file, CkCallbackResumeThread((void*&)result));
    CkPrintf("[Main] Loading Tipsy data and building universe: %lf seconds\n", CkWallTimer() - start_time);

    universe = *((BoundingBox*)result->getData());
    delete result;

#ifdef DEBUG
    std::cout << "[Main] Universal bounding box: " << universe << " with volume " << universe.box.volume() << std::endl;
#endif

    // assign keys and sort particles locally
    start_time = CkWallTimer();
    readers.assignKeys(universe, CkCallbackResumeThread());
    CkPrintf("[Main] Assigning keys and sorting particles: %lf seconds\n", CkWallTimer() - start_time);

    // OCT decomposition: find and sort splitters, send them to Readers for flushing
    // SFC decomposition: globally sort particles (sample sort)
    start_time = CkWallTimer();
    if (decomp_type == OCT_DECOMP) {
      findOctSplitters();
      std::sort(splitters.begin(), splitters.end());
      CkPrintf("[Main] Finding and sorting splitters: %lf seconds\n",
          CkWallTimer() - start_time);
      readers.setSplitters(splitters, CkCallbackResumeThread());
    }
    else if (decomp_type == SFC_DECOMP) {
      //findSfcSplitters();
      //readers.setSplitters(sfc_splitters, bin_counts, CkCallbackResumeThread());
      globalSampleSort();
      CkPrintf("[Main] Global sample sort of particles: %lf seconds\n",
          CkWallTimer() - start_time);

#ifdef DEBUG
      // check if particles are correctly sorted globally
      readers[0].checkSort(Key(0), CkCallbackResumeThread());
#endif
    }

    // create treepieces
    treepieces = CProxy_TreePiece<CentroidData>::ckNew(CkCallbackResumeThread(), universe.n_particles, n_treepieces, n_treepieces);
    CkPrintf("[Main] Created %d TreePieces\n", n_treepieces);

    // flush particles to home TreePieces
    start_time = CkWallTimer();
    if (decomp_type == OCT_DECOMP) {
      readers.flush(universe.n_particles, n_treepieces, treepieces);
      CkStartQD(CkCallbackResumeThread());
    }
    else if (decomp_type == SFC_DECOMP) {
      treepieces.triggerRequest();
      CkStartQD(CkCallbackResumeThread()); // lol is this right
    }
    CkPrintf("[Main] Flushing particles to TreePieces: %lf seconds\n", CkWallTimer() - start_time);

#ifdef DEBUG
    // check if all treepieces have received the right number of particles
    treepieces.check(CkCallbackResumeThread());
#endif

    // free splitter memory
    if (decomp_type == OCT_DECOMP)
      splitters.resize(0);

    // start local tree build in TreePieces
    start_time = CkWallTimer();
    treepieces.build(CkCallbackResumeThread());
    CkPrintf("[Main] Local tree build: %lf seconds\n", CkWallTimer() - start_time);

    // perform downward and upward traversals (Barnes-Hut)
    start_time = CkWallTimer();
    TEHolder<CentroidData> te_holder (centroid_calculator);
    treepieces.upOnly<CentroidVisitor>(te_holder);
  }

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
    findOctSplitters();
    std::sort(splitters.begin(), splitters.end());
    readers.setSplitters(splitters, CkCallbackResumeThread());
    CkPrintf("[Main, iter %d] Finding and sorting splitters: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // flush data to treepieces
    start_time = CkWallTimer();
    readers.flush(universe.n_particles, n_treepieces, treepieces);
    CkWaitQD();
    CkPrintf("[Main, iter %d] Flushing particles to TreePieces: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // rebuild local tree in TreePieces
    start_time = CkWallTimer();
    treepieces.rebuild(CkCallbackResumeThread());
    CkPrintf("[Main, iter %d] Local tree rebuild: %lf seconds\n", cur_iteration, CkWallTimer()-start_time);

    // debug
    CkCallback cb(CkReductionTarget(Main, checkParticlesChangedDone), thisProxy);
    treepieces.checkParticlesChanged(cb);
    CkWaitQD();

    // start traversals
    start_time = CkWallTimer();
    TEHolder<CentroidData> te_holder (centroid_calculator);
    treepieces.upOnly<CentroidVisitor>(te_holder);
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
};

#include "simple.def.h"
