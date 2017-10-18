#include "Decomposer.h"
#include "BufferedVec.h"
#include "Utility.h"
#include <algorithm>
#include <numeric>
#include <bitset>
#include <iostream>

extern CProxy_Main mainProxy;
extern CProxy_Reader readers;
extern CProxy_TreePiece treepieces;
extern std::string input_file;
extern int n_chares;
extern int max_ppc;
extern double decomp_tolerance;
extern int decomp_type;
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

  // find splitters for decomposition
  start_time = CkWallTimer();
  findSplitters();
  CkPrintf("[Decomposer] Finding right splitters: %lf seconds\n", CkWallTimer() - start_time);

  // broadcast finalized splitters and create/initialize treepieces
  if (decomp_type == OCT_DECOMP) {
    readers.setTPKeysAndSplitters(final_splitters, splitters, CkCallbackResumeThread());

    for (int i = 0; i < n_treepieces; i++) {
      treepieces[i].create();
    }

    CkStartQD(CkCallbackResumeThread());
  }
  else if (decomp_type == SFC_DECOMP) {
    readers.setSplitters(final_splitters, bin_counts, CkCallbackResumeThread());

    treepieces.initialize(CkCallbackResumeThread());
  }

  // distribute particles to treepieces
  readers.flush();

  // wait for all particles to be flushed
  CkStartQD(CkCallbackResumeThread());

#ifdef DEBUG
  // check if all treepieces have received the right number of particles
  if (decomp_type == SFC_DECOMP) {
    treepieces.check(CkCallbackResumeThread());
  }
#endif

  // free splitter memory
  splitters.resize(0);

  // start local build of trees in all treepieces
  treepieces.build(CkCallback(CkIndex_Main::terminate(), mainProxy));
}

void Decomposer::findSplitters() {
  if (decomp_type == OCT_DECOMP) {
    /***** OCT DECOMPOSITION ON SFC KEYS *****/
    BufferedVec<Key> keys;

    // initial splitters
    keys.add(Key(1));
    keys.add(~Key(0));
    keys.buffer();

    CkReductionMsg *msg;
    while (true) {
      // send splitters to readers for histogramming
      readers.count(keys.get(), CkCallbackResumeThread((void*&)msg));
      int* counts = (int*)msg->getData();
      int n_counts = msg->getSize() / sizeof(int);

      // check counts and adjust splitters if necessary
      Real threshold = (DECOMP_TOLERANCE * Real(max_ppc));
      for (int i = 0; i < n_counts; i++) {
        Key from = keys.get(2*i);
        Key to = keys.get(2*i+1);

        int n_particles = counts[i];
        if ((Real)n_particles > threshold) {
          keys.add(from << 3);
          keys.add(from << 3 + 1);

          keys.add(from << 3 + 1);
          keys.add(from << 3 + 2);

          keys.add(from << 3 + 2);
          keys.add(from << 3 + 3);

          keys.add(from << 3 + 3);
          keys.add(from << 3 + 4);

          keys.add(from << 3 + 4);
          keys.add(from << 3 + 5);

          keys.add(from << 3 + 5);
          keys.add(from << 3 + 6);

          keys.add(from << 3 + 6);
          keys.add(from << 3 + 7);

          keys.add(from << 3 + 7);
          if (to == (~Key(0)))
            keys.add(~Key(0));
          else
            keys.add(to << 3);
        }
        else {
          splitters.push_back(Utility::shiftLeadingZerosLeft(from)); // for flushing
          final_splitters.push_back(from); // for tree building
          bin_counts.push_back(n_particles);
          std::cout << "splitter: " << std::bitset<64>(from) << ", " << n_particles << std::endl;
        }
      }

      keys.buffer();
      delete msg;

      if (keys.size() == 0)
        break;
    }

    // add largest key as last splitter, used in flushing
    splitters.push_back(~Key(0));

    n_treepieces = final_splitters.size() - 1;
#if DEBUG
    CkPrintf("[Decomposer] Number of TreePieces with OCT decomposition: %d\n", n_treepieces);
#endif

    // TODO need to sort splitters?
  }
  else if (decomp_type == SFC_DECOMP) {
    /***** SFC DECOMPOSITION *****/
    sorted = false;
    // create initial splitters
    Key delta = (SFC::lastPossibleKey - SFC::firstPossibleKey) / n_chares;
    Key splitter = SFC::firstPossibleKey;
    for (int i = 0; i < n_chares; i++, splitter += delta) {
      splitters.push_back(splitter);
    }
    splitters.push_back(SFC::lastPossibleKey);

    // find desired number of particles preceding each splitter
    num_goals_pending = n_chares - 1;
    splitter_goals = new int[num_goals_pending];

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

    // add first key as first splitter
    final_splitters.push_back(SFC::firstPossibleKey);

    // repeat until convergence
    while (1) {
      // histogramming
      num_iterations++;
      readers.count(splitters, CkCallbackResumeThread((void*&)result));
      int* counts = static_cast<int*>(result->getData());
      int n_counts = result->getSize() / sizeof(int);
      bin_counts.resize(n_counts+1);
      bin_counts[0] = 0;
      std::copy(counts, counts + n_counts, bin_counts.begin()+1);
      delete result;

      // prefix sum
      std::partial_sum(bin_counts.begin(), bin_counts.end(), bin_counts.begin());

      // adjust the splitters
      adjustSplitters();

#if DEBUG
      CkPrintf("[Decomposer] Probing %d splitter keys\n", splitters.size());
      CkPrintf("[Decomposer] Decided on %d splitting keys\n", final_splitters.size() -1);
#endif

      if (sorted) {
        CkPrintf("[Decomposer] Histograms balanced after %d iterations\n", num_iterations);

        std::sort(final_splitters.begin(), final_splitters.end());
        final_splitters.push_back(SFC::lastPossibleKey);
        accumulated_bin_counts.push_back(bin_counts.back());
        std::sort(accumulated_bin_counts.begin(), accumulated_bin_counts.end());
        bin_counts.resize(accumulated_bin_counts.size());
        std::adjacent_difference(accumulated_bin_counts.begin(), accumulated_bin_counts.end(), bin_counts.begin());
        accumulated_bin_counts.clear();


#if DEBUG
        CkPrintf("0th Key: %lu, Last Key: %lu\n", final_splitters[0], final_splitters.back());
        CkPrintf("0th Key: %lu, Last Key: %lu\n", SFC::firstPossibleKey, SFC::lastPossibleKey);


        for (int i = 0; i < bin_counts.size(); i++) {
          CkPrintf("[bin %d] %d particles\n", i, bin_counts[i]);
        }
#endif

        sorted = false;
        break;
      }
    }
  }
}

void Decomposer::adjustSplitters() {
  std::vector<Key> new_splitters;
  bins_to_split.Resize(splitters.size() - 1);
  bins_to_split.Zero();
  new_splitters.reserve(splitters.size() * 4);
  new_splitters.push_back(SFC::firstPossibleKey);

  Key left_bound, right_bound;
  vector<int>::iterator num_left_key, num_right_key = bin_counts.begin();

  int num_active_goals = 0;
  // for each goal not achieved yet (i.e splitter key not found)
  for (int i = 0; i < num_goals_pending; i++) {
    //find positions that bracket the goal
    num_right_key = std::lower_bound(num_right_key, bin_counts.end(), splitter_goals[i]);
    num_left_key = num_right_key - 1;

    if (num_right_key == bin_counts.begin())
      ckerr << "Decomposer : Looking for " << splitter_goals[i] << "This is at beginning?" << endl;
    if (num_right_key == bin_counts.end())
      ckerr << "Decomposer : Looking for " << splitter_goals[i] << "This is at end?" << endl;

    //translate positions into bracketing keys
    left_bound = splitters[num_left_key - bin_counts.begin()];
    right_bound = splitters[num_right_key - bin_counts.begin()];

    //check if one of the bounds is close enough to the goal
    if (abs(*num_left_key - splitter_goals[i]) <= tol_diff) {
      // add this key to the list of final splitters
      final_splitters.push_back(left_bound);
      accumulated_bin_counts.push_back(*num_left_key);
    }
    else if (abs(*num_right_key - splitter_goals[i]) <= tol_diff) {
      final_splitters.push_back(right_bound);
      accumulated_bin_counts.push_back(*num_right_key);
    }
    else {
      //not close enough yet, add the bounds and the middle to the probes
      //bottom bits are set to avoid deep trees
      if (new_splitters.back() != (right_bound | 7L)) {
        if (new_splitters.back() != (left_bound | 7L)) {
          new_splitters.push_back(left_bound | 7L);
        }

        new_splitters.push_back((left_bound / 4 * 3 + right_bound / 4) | 7L);
        new_splitters.push_back((left_bound / 2 + right_bound / 2) | 7L);
        new_splitters.push_back((left_bound / 4 + right_bound / 4 * 3) | 7L);
        new_splitters.push_back(right_bound  | 7L);
      }

      splitter_goals[num_active_goals++] = splitter_goals[i];
      bins_to_split.Set(num_left_key - bin_counts.begin());
    }
  }

  num_goals_pending = num_active_goals;

  if (num_goals_pending == 0) {
    sorted = true;
    delete [] splitter_goals;
  }
  else {
    if (new_splitters.back() != SFC::lastPossibleKey) {
      new_splitters.push_back(SFC::lastPossibleKey);
    }
    splitters.reserve(new_splitters.size());
    splitters.assign(new_splitters.begin(), new_splitters.end());
  }
}

