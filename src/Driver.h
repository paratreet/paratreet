#ifndef PARATREET_DRIVER_H_
#define PARATREET_DRIVER_H_

#include "paratreet.decl.h"
#include "common.h"
#include <algorithm>
#include <vector>

#include <numeric>
#include "Reader.h"
#include "Splitter.h"
#include "TreePiece.h"
#include "TreeCanopy.h"
#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Utility.h"
#include "DensityVisitor.h"
#include "GravityVisitor.h"
#include "PressureVisitor.h"
#include "CountVisitor.h"
#include "CacheManager.h"
#include "CountManager.h"
#include "Resumer.h"

extern CProxy_Reader readers;
extern int n_readers;
extern double decomp_tolerance;
extern int max_particles_per_tp; // for OCT decomposition
extern int max_particles_per_leaf; // for local tree build
extern int decomp_type;
extern int tree_type;
extern int num_iterations;
extern int flush_period;
extern CProxy_TreeCanopy<CentroidData> centroid_calculator;
extern CProxy_CacheManager<CentroidData> centroid_cache;
extern CProxy_Resumer<CentroidData> centroid_resumer;
extern CProxy_CountManager count_manager;

template <typename Data>
struct Comparator {
  bool operator() (const std::pair<Key, Data>& a, const std::pair<Key, Data>& b) {return a.first < b.first;}
};

template <typename Data>
class Driver : public CBase_Driver<Data> {
public:
  CProxy_CacheManager<Data> cache_manager;
  std::vector<std::pair<Key, Data>> storage;
  bool storage_sorted;
  BoundingBox universe;
  Key smallest_particle_key;
  Key largest_particle_key;
  std::vector<Splitter> splitters;
  CProxy_TreePiece<CentroidData> treepieces; // Cannot be a global readonly variable
  int n_treepieces;
  double start_time;

  Driver(CProxy_CacheManager<Data> cache_manager_) :
    cache_manager(cache_manager_), storage_sorted(false) {}

  // Performs initial decomposition
  void init(CkCallback cb) {
    // Useful particle keys
    smallest_particle_key = Utility::removeLeadingZeros(Key(1));
    largest_particle_key = (~Key(0));

    CkPrintf("* Initialization\n");
    decompose(0);
    cb.send();
  }

  // Performs decomposition by distributing particles among TreePieces,
  // by either loading particle information from input file or re-computing
  // the universal bounding box
  void decompose(int iter) {
    // Build universe
    start_time = CkWallTimer();
    CkReductionMsg* result;
    if (iter == 0) {
      readers.load(input_file, CkCallbackResumeThread((void*&)result));
      CkPrintf("Loading Tipsy data and building universe: %.3lf ms\n",
          (CkWallTimer() - start_time) * 1000);
    } else {
      readers.computeUniverseBoundingBox(CkCallbackResumeThread((void*&)result));
      CkPrintf("Rebuilding universe: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    }
    universe = *((BoundingBox*)result->getData());
    delete result;

    std::cout << "Universal bounding box: " << universe << " with volume "
      << universe.box.volume() << std::endl;

    // Assign keys and sort particles locally
    start_time = CkWallTimer();
    readers.assignKeys(universe, CkCallbackResumeThread());
    CkPrintf("Assigning keys and sorting particles: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

    // Set up splitters for decomposition
    // TODO: Support decompositions other than OCT
    start_time = CkWallTimer();
    if (decomp_type == OCT_DECOMP) {
      findOctSplitters();
    } else {
      CkAbort("Only OCT decomposition is currently supported");
    }
    std::sort(splitters.begin(), splitters.end());
    readers.setSplitters(splitters, CkCallbackResumeThread());
    CkPrintf("Setting up splitters for decomposition: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

    // Create TreePieces
    start_time = CkWallTimer();
    treepieces = CProxy_TreePiece<CentroidData>::ckNew(CkCallbackResumeThread(),
        universe.n_particles, n_treepieces, centroid_calculator, centroid_resumer,
        centroid_cache, this->thisProxy, n_treepieces);
    CkPrintf("Created %d TreePieces: %.3lf ms\n", n_treepieces,
        (CkWallTimer() - start_time) * 1000);

    // Flush decomposed particles to home TreePieces
    start_time = CkWallTimer();
    readers.flush(universe.n_particles, n_treepieces, treepieces);
    CkStartQD(CkCallbackResumeThread());
    CkPrintf("Flushing particles to TreePieces: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

#if DEBUG
    // Check if all treepieces have received the right number of particles
    treepieces.check(CkCallbackResumeThread());
#endif

    // Free splitter memory
    splitters.clear();
  }

  // Core iterative loop of the simulation
  void run(CkCallback cb) {
    for (int iter = 0; iter < num_iterations; iter++) {
      CkPrintf("\n* Iteration %d\n", iter);

      // Start tree build in TreePieces
      start_time = CkWallTimer();
      if (tree_type == OCT_TREE) {
        treepieces.buildTree();
      } else {
        CkAbort("Only octree is currently supported");
      }
      CkWaitQD();
      CkPrintf("Tree build: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // Prefetch into cache
      start_time = CkWallTimer();
      /*
      centroid_cache.startParentPrefetch(this->thisProxy, centroid_calculator,
          CkCallback::ignore);
      centroid_cache.template startPrefetch<GravityVisitor>(this->thisProxy,
          centroid_calculator, CkCallback::ignore);
      */
      this->thisProxy.loadCache(CkCallbackResumeThread());
      CkWaitQD();
      CkPrintf("TreeCanopy cache loading: %.3lf ms\n",
          (CkWallTimer() - start_time) * 1000);

      // Perform traversals
      start_time = CkWallTimer();
      //treepieces.template startUpAndDown<DensityVisitor>();
      treepieces.template startDown<GravityVisitor>();
      CkWaitQD();
#if DELAYLOCAL
      //treepieces.processLocal(CkCallbackResumeThread());
#endif
      CkPrintf("Tree traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // TODO: Perform interactions
      /*
      start_time = CkWallTimer();
      treepieces.interact(CkCallbackResumeThread());
      CkPrintf("Interactions: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      count_manager.sum(CkCallback(CkReductionTarget(Main, terminate), this->thisProxy));
      */

      // Move the particles in TreePieces
      start_time = CkWallTimer();
      bool complete_rebuild = (iter % flush_period == flush_period-1);
      treepieces.perturb(0.1, complete_rebuild); // 0.1s for example
      CkWaitQD();
      CkPrintf("Perturbations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // Destroy treepices and perform decomposition from scratch
      if (complete_rebuild) {
        treepieces.ckDestroy();
        decompose(iter+1);
      }

      // Clear cache and other storages used in this iteration
      centroid_cache.destroy(true);
      centroid_resumer.destroy();
      storage.clear();
      storage_sorted = false;
      CkWaitQD();
    }

    cb.send();
  }

  // -------------------
  // Auxiliary functions
  // -------------------

  void findOctSplitters() {
    BufferedVec<Key> keys;

    // Initial splitter keys (first and last)
    keys.add(Key(1)); // 0000...1
    keys.add(~Key(0)); // 1111...1
    keys.buffer();

    int decomp_particle_sum = 0; // Used to check if all particles are decomposed

    // Main decomposition loop
    while (keys.size() != 0) {
      // Send splitters to Readers for histogramming
      CkReductionMsg *msg;
      readers.countOct(keys.get(), CkCallbackResumeThread((void*&)msg));
      int* counts = (int*)msg->getData();
      int n_counts = msg->getSize() / sizeof(int);

      // Check counts and create splitters if necessary
      Real threshold = (DECOMP_TOLERANCE * Real(max_particles_per_tp));
      for (int i = 0; i < n_counts; i++) {
        Key from = keys.get(2*i);
        Key to = keys.get(2*i+1);

        int n_particles = counts[i];
        if ((Real)n_particles > threshold) {
          // Create 8 more splitter key pairs to go one level deeper.
          // Leading zeros will be removed in Reader::count() to enable
          // comparison of splitter key and particle key
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
          // Create and store splitter
          Splitter sp(Utility::removeLeadingZeros(from),
              Utility::removeLeadingZeros(to), from, n_particles);
          splitters.push_back(sp);

          // Add up number of particles to check if all are flushed
          decomp_particle_sum += n_particles;
        }
      }

      keys.buffer();
      delete msg;
    }

    // Check if decomposition is correct
    if (decomp_particle_sum != universe.n_particles) {
      CkAbort("Decomposition failure: only %d particles out of %d decomposed",
          decomp_particle_sum, universe.n_particles);
    }

    // Determine number of TreePieces
    // Override input from user if there was one
    n_treepieces = splitters.size();
  }

  void countInts(int* intrn_counts) {
    CkPrintf("%d node-part interactions, %d part-part interactions\n", intrn_counts[0], intrn_counts[1] / 2);
  }

  void recvTC(std::pair<Key, Data> param) {
    storage.emplace_back(param);
  }

  void loadCache(CkCallback cb) {
    CkPrintf("Received data from %d TreeCanopies\n", storage.size());
    CkPrintf("Broadcasting top %d levels to caches\n", num_share_levels);

    // Sort data received from TreeCanopies (by their indices)
    if (!storage_sorted) sortStorage();

    // Find how many should be sent to the caches
    int send_size = storage.size();
    if (num_share_levels >= 0) {
      std::pair<Key, Data> to_search(1 << (LOG_BRANCH_FACTOR * num_share_levels), Data());
      send_size = std::lower_bound(storage.begin(), storage.end(), to_search, Comparator<Data>()) - storage.begin();
    }

    // Send data to caches
    cache_manager.recvStarterPack(storage.data(), send_size, cb);
  }

  void sortStorage() {
    std::sort(storage.begin(), storage.end(), Comparator<Data>());
    storage_sorted = true;
  }

  template <typename Visitor>
  void prefetch(Data nodewide_data, int cm_index, CkCallback cb) {
    // do traversal on the root, send everything
    if (!storage_sorted) sortStorage();
    std::queue<int> node_indices; // better for cache. plus no requirement here on order
    node_indices.push(0);
    std::vector<std::pair<Key, Data>> to_send;
    Visitor v;
    typename std::vector<std::pair<Key, Data> >::iterator it;

    while (node_indices.size()) {
      std::pair<Key, Data> node = storage[node_indices.front()];
      node_indices.pop();
      to_send.push_back(node);

      Node<Data> dummy_node1, dummy_node2;
      dummy_node1.data = node.second;
      dummy_node2.data = nodewide_data;
      if (v.cell(SourceNode<Data>(&dummy_node1), TargetNode<Data>(&dummy_node2))) {

        for (int i = 0; i < BRANCH_FACTOR; i++) {
          Key key = node.first * BRANCH_FACTOR + i;
          it = std::lower_bound(storage.begin(), storage.end(), std::make_pair(key, Data()), Comparator<Data>());
          if (it != storage.end() && it->first == key) {
            node_indices.push(it - storage.begin());
          }
        }
      }
    }
    cache_manager[cm_index].recvStarterPack(to_send.data(), to_send.size(), cb);
  }

  void request(Key* request_list, int list_size, int cm_index, CkCallback cb) {
    if (!storage_sorted) sortStorage();
    typename std::vector<std::pair<Key, Data> >::iterator it;
    std::vector<std::pair<Key, Data>> to_send;
    for (int i = 0; i < list_size; i++) {
      Key key = request_list[i];
      it = std::lower_bound(storage.begin(), storage.end(), std::make_pair(key, Data()), Comparator<Data>());
      if (it != storage.end() && it->first == key) {
        to_send.push_back(*it);
      }
    }
    cache_manager[cm_index].recvStarterPack(to_send.data(), to_send.size(), cb);
  }
};

#endif // PARATREET_DRIVER_H_
