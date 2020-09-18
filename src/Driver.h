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
#include "TreeSpec.h"
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
#include "Modularization.h"
#include "Node.h"
#include "Writer.h"

extern CProxy_Reader readers;
extern CProxy_TreeSpec treespec;
extern CProxy_TreeCanopy<CentroidData> centroid_calculator;
extern CProxy_CacheManager<CentroidData> centroid_cache;
extern CProxy_Resumer<CentroidData> centroid_resumer;
extern CProxy_CountManager count_manager;

template <typename Data>
class Driver : public CBase_Driver<Data> {
private:

public:
  CProxy_CacheManager<Data> cache_manager;
  std::vector<std::pair<Key, SpatialNode<Data>>> storage;
  bool storage_sorted;
  BoundingBox universe;
  Key smallest_particle_key;
  Key largest_particle_key;
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

  void broadcastDecomposition(const CkCallback& cb) {
    PUP::sizer sizer;
    treespec.ckLocalBranch()->getDecomposition()->pup(sizer);
    sizer | const_cast<CkCallback&>(cb);
    CkMarshallMsg *msg = CkAllocateMarshallMsg(sizer.size(), NULL);
    PUP::toMem pupper((void *)msg->msgBuf);
    treespec.ckLocalBranch()->getDecomposition()->pup(pupper);
    pupper | const_cast<CkCallback&>(cb);
    treespec.receiveDecomposition(msg);
  }

  // Performs decomposition by distributing particles among TreePieces,
  // by either loading particle information from input file or re-computing
  // the universal bounding box
  void decompose(int iter) {
    auto config = treespec.ckLocalBranch()->getConfiguration();
    // Build universe
    start_time = CkWallTimer();
    CkReductionMsg* result;
    if (iter == 0) {
      readers.load(config.input_file, CkCallbackResumeThread((void*&)result));
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

    if (universe.n_particles <= config.max_particles_per_tp) {
      CkPrintf("WARNING: Consider using -p to lower max_particles_per_tp, only %d particles.\n",
        universe.n_particles);
    }

    // Assign keys and sort particles locally
    start_time = CkWallTimer();
    readers.assignKeys(universe, CkCallbackResumeThread());
    CkPrintf("Assigning keys and sorting particles: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

    // Set up splitters for decomposition
    start_time = CkWallTimer();
    n_treepieces = treespec.ckLocalBranch()->doFindSplitters(universe, readers);
    broadcastDecomposition(CkCallbackResumeThread());
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
  }

  // Core iterative loop of the simulation
  void run(CkCallback cb) {
    auto config = treespec.ckLocalBranch()->getConfiguration();
    for (int iter = 0; iter < config.num_iterations; iter++) {
      CkPrintf("\n* Iteration %d\n", iter);

      // Start tree build in TreePieces
      start_time = CkWallTimer();
      treepieces.buildTree();
      CkWaitQD();
      CkPrintf("Tree build: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // Prefetch into cache
      start_time = CkWallTimer();
      // use exactly one of these three commands to load the software cache
      //centroid_cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
      //centroid_cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
      this->thisProxy.loadCache(CkCallbackResumeThread());
      CkWaitQD();
      CkPrintf("TreeCanopy cache loading: %.3lf ms\n",
          (CkWallTimer() - start_time) * 1000);

      // Perform traversals
      start_time = CkWallTimer();
      //treepieces.template startUpAndDown<DensityVisitor>();
      //treepieces.template startDown<GravityVisitor>();
      treespec.ckLocalBranch()->getConfiguration().traversalFn(treepieces, iter);
      CkWaitQD();
#if DELAYLOCAL
      //treepieces.processLocal(CkCallbackResumeThread());
#endif
      CkPrintf("Tree traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // Perform interactions
      start_time = CkWallTimer();
      treepieces.interact(CkCallbackResumeThread());
      CkPrintf("Interactions: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      //count_manager.sum(CkCallback(CkReductionTarget(Main, terminate), this->thisProxy));

      // Move the particles in TreePieces
      start_time = CkWallTimer();
      bool complete_rebuild = (iter % config.flush_period == config.flush_period - 1);
      treepieces.perturb(config.timestep_size, complete_rebuild); // 0.1s for example
      CkWaitQD();
      CkPrintf("Perturbations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // Call user's post-interaction function, which may for example:
      // Output particle accelerations for verification
      // TODO: Initial force interactions similar to ChaNGa
      treespec.ckLocalBranch()->getConfiguration().postInteractionsFn(universe, treepieces, iter);

      // Destroy treepieces and perform decomposition from scratch
      if (complete_rebuild) {
        treepieces.destroy();
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

  void countInts(unsigned long long* intrn_counts) {
    CkPrintf("%llu node-particle interactions, %llu particle-particle interactions\n", intrn_counts[0], intrn_counts[1]);
  }

  void recvTC(std::pair<Key, SpatialNode<Data>> param) {
    storage.emplace_back(param);
  }

  void loadCache(CkCallback cb) {
    auto config = treespec.ckLocalBranch()->getConfiguration();
    CkPrintf("Received data from %d TreeCanopies\n", (int) storage.size());
    // Sort data received from TreeCanopies (by their indices)
    if (!storage_sorted) sortStorage();

    // Find how many should be sent to the caches
    int send_size = storage.size();
    if (config.num_share_nodes > 0 && config.num_share_nodes < send_size) {
      send_size = config.num_share_nodes;
    }
    else {
      CkPrintf("Broadcasting every tree canopy because num_share_nodes is unset\n");
    }
    // Send data to caches
    cache_manager.recvStarterPack(storage.data(), send_size, cb);
  }

  void sortStorage() {
    auto comp = [] (const std::pair<Key, SpatialNode<Data>>& a, const std::pair<Key, SpatialNode<Data>>& b) {return a.first < b.first;};
    std::sort(storage.begin(), storage.end(), comp);
    storage_sorted = true;
  }

  template <typename Visitor>
  void prefetch(Data nodewide_data, int cm_index, CkCallback cb) { // TODO
    CkAssert(false);
    // do traversal on the root, send everything
    /*if (!storage_sorted) sortStorage();
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
      if (v.cell(*dummy_node1, *dummy_node2)) {

        for (int i = 0; i < BRANCH_FACTOR; i++) {
          Key key = node.first * BRANCH_FACTOR + i;
          auto comp = [] (auto && a, auto && b) {return a.first < b.first;};
          it = std::lower_bound(storage.begin(), storage.end(), std::make_pair(key, Data()), comp);
          if (it != storage.end() && it->first == key) {
            node_indices.push(std::distance(storage.begin(), it));
          }
        }
      }
    }
    cache_manager[cm_index].recvStarterPack(to_send.data(), to_send.size(), cb);
    */
  }

  void request(Key* request_list, int list_size, int cm_index, CkCallback cb) {
    if (!storage_sorted) sortStorage();
    std::vector<std::pair<Key, SpatialNode<Data>>> to_send;
    for (int i = 0; i < list_size; i++) {
      Key key = request_list[i];
      auto comp = [] (const std::pair<Key, SpatialNode<Data>>& a, const Key & b) {return a.first < b;};
      auto it = std::lower_bound(storage.begin(), storage.end(), key, comp);
      if (it != storage.end() && it->first == key) {
        to_send.push_back(*it);
      }
    }
    cache_manager[cm_index].recvStarterPack(to_send.data(), to_send.size(), cb);
  }
};

#endif // PARATREET_DRIVER_H_
