#include "Decomposer.h"
#include "BufferedVec.h"
#include <algorithm>

extern CProxy_Main mainProxy;
extern CProxy_Reader readers;
extern CProxy_TreePiece treepieces;
extern std::string input_file;
extern int n_chares;
extern double decomp_tolerance;
extern int tree_type;

Decomposer::Decomposer() {
  result = NULL;
}

void Decomposer::run() {
  // load Tipsy data and build universe
  start_time = CkWallTimer();
  readers.load(input_file, CkCallbackResumeThread((void*&)result));
  CkPrintf("[Decomposer] Loading Tipsy data and building universe: %lf seconds\n", CkWallTimer() - start_time);

  universe = *((BoundingBox*)result->getData());
  delete result;

#ifdef DEBUG
  cout << "[Decomposer] Universal bounding box: " << universe << endl;
#endif

  // assign keys and sort particles locally
  start_time = CkWallTimer();
  readers.assignKeys(universe, CkCallbackResumeThread());
  CkPrintf("[Decomposer] Assigning keys and sorting particles: %lf seconds\n", CkWallTimer() - start_time);

  // TODO find splitters that (almost) equally split particles
  start_time = CkWallTimer();
  findSplitters();
  CkPrintf("[Decomposer] Finding right splitters: %lf seconds\n", CkWallTimer() - start_time);

  // initialize TreePieces
  treepieces.initialize(CkCallbackResumeThread());

  // have the readers distribute particles to treepieces
  readers.flush();

  // wait for all particles to be flushed
  CkStartQD(CkCallbackResumeThread());

#ifdef DEBUG
  // check if all treepieces have received the right number of particles
  treepieces.check(CkCallbackResumeThread());
#endif

  // free splitter memory
  splitters.resize(0);

  // start local build of trees in all treepieces
  treepieces.build(CkCallback(CkIndex_Main::terminate(), mainProxy));
}

void Decomposer::findSplitters() {
  // create initial splitters
  Key delta = (SFC::lastPossibleKey - SFC::firstPossibleKey) / n_chares;
  Key splitter = SFC::firstPossibleKey;
  for (int i = 0; i < n_chares; i++, splitter += delta) {
    splitters.push_back(splitter);
  }
  splitters.push_back(SFC::lastPossibleKey);

  // find desired number of particles preceding each splitter
  int* splitter_goals = new int[n_chares];
  int avg = universe.n_particles / n_chares;
  int rem = universe.n_particles % n_chares;
  int prev = 0;
  for (int i = 0; i < rem; i++) {
    splitter_goals[i] = prev + avg + 1;
    prev = splitter_goals[i];
  }
  for (int i = rem; i < n_chares; i++) {
    splitter_goals[i] = prev + avg;
    prev = splitter_goals[i];
  }

  // calculate tolerated difference in # of particles
  tol_diff = static_cast<int>(avg * decomp_tolerance);

  // repeat until convergence
  while (1) {
    // histogramming
    readers.count(splitters, CkCallbackResumeThread((void*&)result));
    int* counts = static_cast<int*>(result->getData());
    int n_counts = result->getSize() / sizeof(int);
    bin_counts.resize(n_counts+1);
    bin_counts[0] = 0;
    std::copy(counts, counts + n_counts, bin_counts.begin()+1);
    delete result;

    // prefix sum
    std::partial_sum(bin_counts.begin(), bin_counts.end(), bin_counts.begin());

    // TODO adjustSplitters
    break;
  }

  /*
  CkReductionMsg* result = NULL;
  int num_splitters;

  BufferedVec<Key> keys;
  keys.add(Key(1));
  keys.add(~Key(0));
  keys.buffer();

  // SFC decomposition
  while (1) {
    readers.count(keys.get(), CkCallbackResumeThread((void*&)result));
    int* counts = (int*)result->getData();
    int n_counts = result->getSize() / sizeof(int);

    CkAssert(2 * n_counts == keys.size());

    Real threshold = (Real)max_ppc * DECOMP_TOLERANCE;
    for (int i = 0; i < n_counts; i++) {
      Key from = keys.get(2*i);
      Key to = keys.get(2*i+1);

      int np = counts[i];
      if ((Real)np > threshold) {
        keys.add(from << 1);
        keys.add((from << 1)+1);
        keys.add((from << 1)+1);

        if (to == (~Key(0))) {
          keys.add(~Key(0));
        }
        else {
          keys.add(to << 1);
        }
      }
      else {
        splitters.push_back(Splitter(Splitter::convertKey(from), Splitter::convertKey(to), from, np));
      }
    }

    keys.buffer();
    delete result;

    if (keys.size() == 0)
      break;
  }

  // sort splitters and send them to readers
  splitters.quickSort();
  readers.setSplitters(splitters, CkCallbackResumeThread());
  n_splitters = splitters.size();
  */
}
