#include "Decomposer.h"
#include "BufferedVec.h"
#include "Utility.h"
#include <algorithm>
#include <numeric>
#include <bitset>
#include <iostream>

extern CProxy_Main mainProxy;
extern CProxy_Reader readers;
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
 
  // sort splitters for correct flushing
  if (decomp_type == OCT_DECOMP) splitters.quickSort(); // final_splitters are already sorted
  CkPrintf("[Decomposer] Sorting splitters: %lf seconds\n", CkWallTimer() - start_time);

  // send finalized splitters to readers
  if (decomp_type == OCT_DECOMP) readers.setSplitters(splitters, CkCallbackResumeThread());
  else if (decomp_type == SFC_DECOMP) readers.setSplitters(final_splitters, bin_counts, CkCallbackResumeThread());

  // create treepieces
  treepieces = CProxy_TreePiece::ckNew(CkCallbackResumeThread(), decomp_type == OCT_DECOMP, n_treepieces);

  // flush particles with finalized splitters
  start_time = CkWallTimer();
  readers.flush(treepieces);

  // wait for all particles to be flushed
  CkStartQD(CkCallbackResumeThread());
  CkPrintf("[Decomposer] Flushing particles: %lf seconds\n", CkWallTimer() - start_time);

#ifdef DEBUG
  // check if all treepieces have received the right number of particles
  treepieces.check(CkCallbackResumeThread());
#endif

  // free splitter memory
  splitters.resize(0);

  // start local build of trees in all treepieces
  start_time = CkWallTimer();
  //treepieces.build(CkCallbackResumeThread());
  CkPrintf("[Decomposer] Local tree build: %lf seconds\n", CkWallTimer() - start_time);

  mainProxy.terminate();
}

void Decomposer::findSplitters() {
  if (decomp_type == OCT_DECOMP) {
    /***** OCT DECOMPOSITION ON SFC KEYS *****/
    BufferedVec<Key> keys;

    // initial splitters
    keys.add(Key(1));
    keys.add(~Key(0));
    keys.buffer();

    int decomp_sum = 0;

    tree_array = CProxy_TreeElements::ckNew();

    CkReductionMsg *msg;
    while (true) {
      // send splitters to readers for histogramming
      readers.count(keys.get(), CkCallbackResumeThread((void*&)msg));
      int* counts = (int*)msg->getData();
      int n_counts = msg->getSize() / sizeof(int);

      // check counts and create splitters if necessary
      Real threshold = (DECOMP_TOLERANCE * Real(max_ppc));
      for (int i = 0; i < n_counts; i++) {
        Key from = keys.get(2*i);
        Key to = keys.get(2*i+1);

        int n_particles = counts[i];
        if ((Real)n_particles > threshold) {
          tree_array[from].exist(false);

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
          /*
          keys.add(from << 1);
          keys.add((from << 1) + 1);
          keys.add((from << 1) + 1);
          if (to == (~Key(0)))
            keys.add(~Key(0));
          else
            keys.add(to << 1);
          */
        }
        else {

            // this is a splitter/treepiece, again from is the index that matters
          tree_array[from].exist(true);

          // create and store splitter set
          Splitter sp(Utility::removeLeadingZeros(from), Utility::removeLeadingZeros(to), from, n_particles);
          splitters.push_back(sp);
          //std::cout << "[" << std::bitset<64>(Utility::removeLeadingZeros(from)) << ", " << std::bitset<64>(Utility::removeLeadingZeros(to)) << "]" << std::endl;

          // add up number of particles to check if all are flushed
          decomp_sum += n_particles;
        }
      }

      keys.buffer();
      delete msg;

      if (keys.size() == 0)
        break;
    }

    n_treepieces = splitters.size();
#if DEBUG
    CkPrintf("[Decomposer] %d particles decomposed\n", decomp_sum);
    CkPrintf("[Decomposer] Number of TreePieces with OCT decomposition: %d\n", n_treepieces);
#endif
  }
  else if (decomp_type == SFC_DECOMP) {
    /***** SFC DECOMPOSITION *****/

    sorted = false;
    // create initial splitters
    Key delta = (SFC::lastPossibleKey - SFC::firstPossibleKey) / n_chares; // decide how much distance to cover
    Key splitter = SFC::firstPossibleKey;
    for (int i = 0; i < n_chares; i++, splitter += delta) {
      ksplitters.push_back(splitter); // splitter[i] = delta * i
    }
    ksplitters.push_back(SFC::lastPossibleKey);
      // there are n + 1 because they are used as a range

    // find desired number of particles preceding each splitter
    num_goals_pending = n_chares - 1;
    splitter_goals = new int[num_goals_pending];

    int avg = universe.n_particles / n_chares;
    int rem = universe.n_particles % n_chares;
    int prev = 0;
    for (int i = 0; i < rem; i++) {
      splitter_goals[i] = prev + avg + 1; // add the prev so we can use prefix sum
      prev = splitter_goals[i];
    }
    for (int i = rem; i < n_chares; i++) {
      splitter_goals[i] = prev + avg;
      prev = splitter_goals[i];
    }
      // determine how many particles should be in each chare

    // calculate tolerated difference in # of particles
    tol_diff = static_cast<int>(avg * decomp_tolerance);
      // difference scaled by average

    // add first key as first splitter
    final_splitters.push_back(SFC::firstPossibleKey);
      // the first one is used as beginning of a range

    // repeat until convergence
    while (1) {
      // histogramming
      num_iterations++; // just a record
      readers.count(ksplitters, CkCallbackResumeThread((void*&)result)); // let it see how many fit into each bin
      int* counts = static_cast<int*>(result->getData()); // we got result, now lets access it
      int n_counts = result->getSize() / sizeof(int);
      bin_counts.resize(n_counts+1); //  this stuff just makes our own copy of result
      bin_counts[0] = 0; // range causes n + 1, so we set the first one to 0. i think ??
      std::copy(counts, counts + n_counts, bin_counts.begin()+1);
      delete result;

      // prefix sum
      std::partial_sum(bin_counts.begin(), bin_counts.end(), bin_counts.begin());

      // adjust the splitters
      adjustSplitters(); // is adjusting the best word? it seems like they're just spreading things out

#if DEBUG
      CkPrintf("[Decomposer] Probing %d splitter keys\n", ksplitters.size());
      CkPrintf("[Decomposer] Decided on %d splitting keys\n", final_splitters.size() -1);
#endif

      if (sorted) {
        CkPrintf("[Decomposer] Histograms balanced after %d iterations\n", num_iterations);

        ksplitters.resize(0);
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
  bins_to_split.Resize(ksplitters.size() - 1);
  bins_to_split.Zero();
  new_splitters.reserve(ksplitters.size() * 4); // there are 4 because were adding four more
    // but when do we get rid of them?
    // perhaps using accumulated bin counts
  new_splitters.push_back(SFC::firstPossibleKey);

  Key left_bound, right_bound;
  vector<int>::iterator num_left_key, num_right_key = bin_counts.begin();

  int num_active_goals = 0;
  // for each goal not achieved yet (i.e splitter key not found)
  for (int i = 0; i < num_goals_pending; i++) {
    //find positions that bracket the goal
    num_right_key = std::lower_bound(num_right_key, bin_counts.end(), splitter_goals[i]);
    num_left_key = num_right_key - 1;
      // dividing the ones whose bin count is high is great, but what if
      // there aren't any bin counts greater than the goal? they are too fine then?
      // ordering of the acc bin counts, ordering of final splitters, might have problems
      // the order is probably what makes the flushing so difficult

    if (num_right_key == bin_counts.begin())
      ckerr << "Decomposer : Looking for " << splitter_goals[i] << "This is at beginning?" << endl;
    if (num_right_key == bin_counts.end())
      ckerr << "Decomposer : Looking for " << splitter_goals[i] << "This is at end?" << endl;

    //translate positions into bracketing keys
    left_bound = ksplitters[num_left_key - bin_counts.begin()];
    right_bound = ksplitters[num_right_key - bin_counts.begin()];

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
          new_splitters.push_back(left_bound | 7L); // this is left
        }

        new_splitters.push_back((left_bound / 4 * 3 + right_bound / 4) | 7L); // this is left heavy
        new_splitters.push_back((left_bound / 2 + right_bound / 2) | 7L); // this is halfway
        new_splitters.push_back((left_bound / 4 + right_bound / 4 * 3) | 7L); // this is right heavy
        new_splitters.push_back(right_bound  | 7L); // this is right
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
    ksplitters.reserve(new_splitters.size());
    ksplitters.assign(new_splitters.begin(), new_splitters.end());
  }
}

