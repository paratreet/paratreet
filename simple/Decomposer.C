#include "Decomposer.h"
#include "BufferedVec.h"
#include "Utility.h"
#include <algorithm>
#include <numeric>
#include <bitset>
#include <iostream>

extern CProxy_Main mainProxy;
extern CProxy_Reader readers;
extern int n_readers;
extern std::string input_file;
extern int max_particles_per_tp;
extern double decomp_tolerance;
extern int decomp_type;
extern int tree_type;

Decomposer::Decomposer(int n_treepieces) {
  this->n_treepieces = n_treepieces;
  smallest_particle_key = Utility::removeLeadingZeros(Key(1));
  largest_particle_key = (~Key(0));
}

void Decomposer::run() {
  // load Tipsy data and build universe
  double start_time = CkWallTimer();
  CkReductionMsg* result;
  readers.load(input_file, CkCallbackResumeThread((void*&)result));
  CkPrintf("[Decomposer] Loading Tipsy data and building universe: %lf seconds\n", CkWallTimer() - start_time);

  universe = *((BoundingBox*)result->getData());
  delete result;

#ifdef DEBUG
  std::cout << "[Decomposer] Universal bounding box: " << universe << std::endl;
#endif

  // assign keys and sort particles locally
  start_time = CkWallTimer();
  readers.assignKeys(universe, CkCallbackResumeThread());
  CkPrintf("[Decomposer] Assigning keys and sorting particles: %lf seconds\n", CkWallTimer() - start_time);

  // OCT decomposition: find splitters
  // SFC decomposition: globally sort particles (sample sort)
  start_time = CkWallTimer();
  if (decomp_type == OCT_DECOMP) {
    findOctSplitters();
    CkPrintf("[Decomposer] Determining splitters: %lf seconds\n",
        CkWallTimer() - start_time);
  }
  else if (decomp_type == SFC_DECOMP) {
    //findSfcSplitters();
    globalSampleSort();
    CkPrintf("[Decomposer] Global sample sort of particles: %lf seconds\n",
        CkWallTimer() - start_time);

#ifdef DEBUG
    // check if particles are correctly sorted globally
    readers[0].checkSort(Key(0), CkCallbackResumeThread());
#endif
  }

  /*
  // sort splitters for correct flushing
  if (decomp_type == OCT_DECOMP) oct_splitters.quickSort();
  CkPrintf("[Decomposer] Sorting splitters: %lf seconds\n", CkWallTimer() - start_time);

  // send finalized splitters to readers
  if (decomp_type == OCT_DECOMP) readers.setSplitters(oct_splitters, CkCallbackResumeThread());
  else if (decomp_type == SFC_DECOMP) readers.setSplitters(sfc_splitters, bin_counts, CkCallbackResumeThread());

  // create treepieces
  treepieces = CProxy_TreePiece::ckNew(CkCallbackResumeThread(), decomp_type == OCT_DECOMP, n_treepieces);
  CkPrintf("[Decomposer] Created %d TreePieces\n");

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
  if (decomp_type == OCT_DECOMP) oct_splitters.resize(0);
  else if (decomp_type == SFC_DECOMP) sfc_splitters.resize(0);

  // start local build of trees in all treepieces
  start_time = CkWallTimer();
  //treepieces.build(CkCallbackResumeThread());
  CkPrintf("[Decomposer] Local tree build: %lf seconds\n", CkWallTimer() - start_time);
  */

  mainProxy.terminate();
}

void Decomposer::findOctSplitters() {
  BufferedVec<Key> keys;

  // initial splitter keys (first and last)
  keys.add(Key(1)); // 0000...1
  keys.add(~Key(0)); // 1111...1
  keys.buffer();

  // create placeholder for TreeElement chares
  tree_array = CProxy_TreeElements::ckNew();

  int decomp_particle_sum = 0; // to check if all particles are decomposed

  // main decomposition loop
  while (keys.size() != 0) {
    // send splitters to readers for histogramming
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
        // create corresponding TreeElement (not a TreePiece)
        tree_array[from].exist(false);

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
        // this is a splitter/TreePiece, again from is the index that matters
        tree_array[from].exist(true);

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
    CkPrintf("[Decomposer] Only %d particles out of %d were decomposed!\n",
        decomp_particle_sum, universe.n_particles);
    CkAbort("Decomposition error");
  }

  // determine number of TreePieces
  n_treepieces = splitters.size();
}

void Decomposer::globalSampleSort() {
  // perform parallel sample sort of all particle keys
  int avg_n_particles = universe.n_particles / n_treepieces;
  int oversampling_ratio = 10;
  if (avg_n_particles < oversampling_ratio)
    oversampling_ratio = avg_n_particles;

#ifdef DEBUG
  CkPrintf("[Decomposer] Oversampling ratio: %d\n", oversampling_ratio);
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
  CkPrintf("[Decomposer] Splitter keys for sorting:\n");
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

/*
void Decomposer::findSfcSplitters() {
  // create candidate splitters that are equally apart
  // there are n + 1 splliters because they are used as a range
  std::vector<Key> candidate_splitter_keys;
  Key delta = (largest_particle_key - smallest_particle_key) / n_treepieces;
  Key splitter_key = smallest_particle_key;
  for (int i = 0; i < n_treepieces; i++, splitter_key += delta) {
    candidate_splitter_keys.push_back(splitter_key);
  }
  candidate_splitter_keys.push_back(largest_particle_key);

  // determine how many particles should be in each TreePiece,
  // perform prefix-sum and store in splitter_goals
  int n_goals_pending = n_treepieces;
  std::vector<int> splitter_goals(n_goals_pending);

  int avg = universe.n_particles / n_treepieces;
  int rem = universe.n_particles % n_treepieces;
  int prev = 0;
  for (int i = 0; i < rem; i++) {
    splitter_goals[i] = prev + avg + 1;
    prev = splitter_goals[i];
  }
  for (int i = rem; i < n_goals_pending; i++) {
    splitter_goals[i] = prev + avg;
    prev = splitter_goals[i];
  }

#ifdef DEBUG
  CkPrintf("[Decomposer] %d splitter goals:", n_treepieces);
  for (int i = 0; i < n_treepieces; i++) {
    CkPrintf(" %d", splitter_goals[i]);
  }
  CkPrintf("\n");
#endif

  // compute tolerated difference of particle counts
  int tol_diff = static_cast<int>(avg * decomp_tolerance);

  // first SFC key serves as the starting range
  std::vector<Key> final_splitter_keys;
  final_splitter_keys.push_back(smallest_particle_key);

  // prepare histogramming
  bool histogram_balanced = false;
  int n_iters = 0;
  std::vector<int> bin_counts;
  std::vector<int> accumulated_bin_counts;

  // repeat until satisfactory splitters are found
  while (!histogram_balanced) {
    ++n_iters;

    // let's see how many particles fit into each bin
    CkReductionMsg* result;
    readers.countSfc(candidate_splitter_keys, CkCallbackResumeThread((void*&)result));

    // prepare bin_counts for prefix sum
    int* counts = static_cast<int*>(result->getData());
    int n_counts = result->getSize() / sizeof(int);
    bin_counts.resize(n_counts+1);
    bin_counts[0] = 0; // for exclusive prefix sum
    std::copy(counts, counts + n_counts, bin_counts.begin()+1);
    delete result;

    // perform exclusive prefix sum
    std::partial_sum(bin_counts.begin(), bin_counts.end(), bin_counts.begin());

    // modify the splitters to get a better balanced histogram
    // TODO need to revisit algorithm - it might be better to do a parallel sort instead
    histogram_balanced = modifySfcSplitters(candidate_splitter_keys,
        final_splitter_keys, splitter_goals, n_goals_pending,
        bin_counts, accumulated_bin_counts, tol_diff);
    readers.globalSampleSort(

#ifdef DEBUG
    CkPrintf("[Decomposer] Histogram iteration %d\n", n_iters);
    CkPrintf("[Decomposer] Probing %d candidate splitters\n",
        candidate_splitter_keys.size());
    CkPrintf("[Decomposer] Decided on %d splitting keys\n",
        final_splitter_keys.size()-1);
#endif
  }

  CkPrintf("[Decomposer] Histograms balanced after %d iterations\n", n_iters);

  // TODO check if these work as intended
  std::sort(final_splitter_keys.begin(), final_splitter_keys.end());
  final_splitter_keys.push_back(SFC::lastPossibleKey);
  accumulated_bin_counts.push_back(bin_counts.back());
  std::sort(accumulated_bin_counts.begin(), accumulated_bin_counts.end());
  bin_counts.resize(accumulated_bin_counts.size());
  std::adjacent_difference(accumulated_bin_counts.begin(), accumulated_bin_counts.end(), bin_counts.begin());
  accumulated_bin_counts.clear();
}

bool Decomposer::modifySfcSplitters(std::vector<Key>& candidate_splitter_keys,
    std::vector<Key>& final_splitter_keys, std::vector<int>& splitter_goals,
    int& n_goals_pending, std::vector<int>& bin_counts,
    std::vector<int>& accumulated_bin_counts, const int tol_diff) {
  // new set of splitter keys
  std::vector<Key> new_splitter_keys;
  new_splitter_keys.reserve(candidate_splitter_keys.size() * 4);
  new_splitter_keys.push_back(smallest_particle_key);

  // keys and iterators for bracketing the goal
  Key left_bound, right_bound;
  std::vector<int>::iterator bin_it_left, bin_it_right = bin_counts.begin();

  CkBitVector bins_to_split;
  bins_to_split.Resize(candidate_splitter_keys.size() - 1);
  bins_to_split.Zero();

  int n_active_goals = 0;
  // for each goal not achieved yet (i.e. splitter key not finalized)
  for (int i = 0; i < n_goals_pending; i++) {
    // find positions that bracket the goal
    bin_it_right = std::lower_bound(bin_it_right, bin_counts.end(), splitter_goals[i]);
    bin_it_left = bin_it_right - 1;

    // dividing the ones whose bin count is high is great, but what if
    // there aren't any bin counts greater than the goal? they are too fine then?
    // ordering of the acc bin counts, ordering of final splitters, might have problems
    // the order is probably what makes the flushing so difficult

    if (bin_it_right == bin_counts.begin())
      CkPrintf("[Decomposer] Looking for %d, this is at beginning?\n", splitter_goals[i]);
    else if (bin_it_right == bin_counts.end())
      CkPrintf("[Decomposer] Looking for %d, this is at end?\n", splitter_goals[i]);

    // translate positions into bracketing keys
    left_bound = candidate_splitter_keys[bin_it_left - bin_counts.begin()];
    right_bound = candidate_splitter_keys[bin_it_right - bin_counts.begin()];

    // check if one of the bounds is close enough to the goal
    if (abs(*bin_it_left - splitter_goals[i]) <= tol_diff) {
      // add this key to the list of final splitters
      final_splitter_keys.push_back(left_bound);
      accumulated_bin_counts.push_back(*bin_it_left);
    }
    else if (abs(*bin_it_right - splitter_goals[i]) <= tol_diff) {
      final_splitter_keys.push_back(right_bound);
      accumulated_bin_counts.push_back(*bin_it_right);
    }
    else {
      // not close enough yet, add the bounds and the middle to the probes
      // bottom bits are set to avoid deep trees
      if (new_splitter_keys.back() != (right_bound | 7L)) {
        if (new_splitter_keys.back() != (left_bound | 7L)) {
          new_splitter_keys.push_back(left_bound | 7L); // this is left
        }

        new_splitter_keys.push_back((left_bound / 4 * 3 + right_bound / 4) | 7L); // this is left heavy
        new_splitter_keys.push_back((left_bound / 2 + right_bound / 2) | 7L); // this is halfway
        new_splitter_keys.push_back((left_bound / 4 + right_bound / 4 * 3) | 7L); // this is right heavy
        new_splitter_keys.push_back(right_bound  | 7L); // this is right
      }

      splitter_goals[n_active_goals++] = splitter_goals[i];
      bins_to_split.Set(bin_it_left - bin_counts.begin());
    }
  }

  n_goals_pending = n_active_goals;

  if (n_goals_pending == 0) {
    return true;
  }
  else {
    if (new_splitter_keys.back() != SFC::lastPossibleKey) {
      new_splitter_keys.push_back(SFC::lastPossibleKey);
    }
    final_splitter_keys.reserve(new_splitter_keys.size());
    final_splitter_keys.assign(new_splitter_keys.begin(), new_splitter_keys.end());
  }

  return false;
}
*/
