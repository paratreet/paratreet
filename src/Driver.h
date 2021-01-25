#ifndef PARATREET_DRIVER_H_
#define PARATREET_DRIVER_H_

#include "paratreet.decl.h"
#include "common.h"
#include <algorithm>
#include <vector>

#include <numeric>
#include "Reader.h"
#include "Splitter.h"
#include "TreeCanopy.h"
#include "TreeSpec.h"
#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Utility.h"
#include "CacheManager.h"
#include "Resumer.h"
#include "Modularization.h"
#include "Node.h"
#include "Writer.h"
#include "Subtree.h"

extern CProxy_Reader readers;
extern CProxy_TreeSpec treespec;
extern CProxy_TreeCanopy<CentroidData> centroid_calculator;
extern CProxy_CacheManager<CentroidData> centroid_cache;
extern CProxy_Resumer<CentroidData> centroid_resumer;

namespace paratreet {
  extern void preTraversalFn(CProxy_Driver<CentroidData>&, CProxy_CacheManager<CentroidData>& cache);
  extern void traversalFn(BoundingBox&,CProxy_Partition<CentroidData>&,int);
  extern void postInteractionsFn(BoundingBox&,CProxy_Partition<CentroidData>&,int);
}

template <typename Data>
class Driver : public CBase_Driver<Data> {
private:

public:
  CProxy_CacheManager<Data> cache_manager;
  std::vector<std::pair<Key, SpatialNode<Data>>> storage;
  bool storage_sorted;
  BoundingBox universe;
  CProxy_Subtree<CentroidData> subtrees; // Cannot be a global readonly variable
  CProxy_Partition<CentroidData> partitions;
  int n_subtrees;
  int n_partitions;
  double start_time;
  Real max_velocity;
  Real updated_timestep_size = 0.1;

  Driver(CProxy_CacheManager<Data> cache_manager_) :
    cache_manager(cache_manager_), storage_sorted(false) {}

  // Performs initial decomposition
  void init(CkCallback cb) {
    // Ensure all treespecs have been created
    CkPrintf("* Validating tree specifications.\n");
    treespec.check(CkCallbackResumeThread());
    // Then, initialize the cache managers
    CkPrintf("* Initializing cache managers.\n");
    cache_manager.initialize(CkCallbackResumeThread());
    // Useful particle keys
    CkPrintf("* Initialization\n");
    decompose(0);
    cb.send();
  }

  // Performs decomposition by distributing particles among Subtrees,
  // by either loading particle information from input file or re-computing
  // the universal bounding box
  void decompose(int iter) {
    auto config = treespec.ckLocalBranch()->getConfiguration();
    // Build universe
    double decomp_time = CkWallTimer();
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

    Vector3D<Real> bsize = universe.box.size();
    Real max = (bsize.x > bsize.y) ? bsize.x : bsize.y;
    max = (max > bsize.z) ? max : bsize.z;
    Vector3D<Real> bcenter = universe.box.center();
    // The magic number below is approximately 2^(-19)
    const Real fEps = 1.0 + 1.91e-6;  // slop to ensure keys fall between 0 and 1.
    bsize = Vector3D<Real>(fEps*0.5*max);
    universe.box = OrientedBox<Real>(bcenter-bsize, bcenter+bsize);

    std::cout << "Universal bounding box: " << universe << " with volume "
      << universe.box.volume() << std::endl;

    if (config.min_n_subtrees < CkNumPes() || config.min_n_partitions < CkNumPes()) {
      CkPrintf("WARNING: Consider increasing min_n_subtrees and min_n_partitions to at least #pes\n");
    }

    // Assign keys and sort particles locally
    start_time = CkWallTimer();
    readers.assignKeys(universe, CkCallbackResumeThread());
    CkPrintf("Assigning keys and sorting particles: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

    // Set up splitters for decomposition
    start_time = CkWallTimer();
    n_subtrees = treespec.ckLocalBranch()->getSubtreeDecomposition()->findSplitters(universe, readers, config.min_n_subtrees);
    treespec.receiveDecomposition(CkCallbackResumeThread(),
      CkPointer<Decomposition>(treespec.ckLocalBranch()->getSubtreeDecomposition()), true);
    bool matching_decomps = config.decomp_type == paratreet::subtreeDecompForTree(config.tree_type);
    if (matching_decomps) {
      n_partitions = n_subtrees;
      CkPrintf("Using same decomposition for subtrees and partitions\n");
      treespec.receiveDecomposition(CkCallbackResumeThread(),
        CkPointer<Decomposition>(treespec.ckLocalBranch()->getSubtreeDecomposition()), false);
    }
    else {
      n_partitions = treespec.ckLocalBranch()->getPartitionDecomposition()->findSplitters(universe, readers, config.min_n_partitions);
      // partition doFindSplitters + subtree doFind do not depend on each other
      // only dependency is: partition flush must go before subtree flush
      treespec.receiveDecomposition(CkCallbackResumeThread(),
        CkPointer<Decomposition>(treespec.ckLocalBranch()->getPartitionDecomposition()), false);
    }
    CkPrintf("Setting up splitters for particle decompositions: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

    // Create Subtrees
    start_time = CkWallTimer();
    CkArrayOptions subtree_opts(n_subtrees);
    treespec.ckLocalBranch()->getSubtreeDecomposition()->setArrayOpts(subtree_opts);
    subtrees = CProxy_Subtree<CentroidData>::ckNew(
      CkCallbackResumeThread(),
      universe.n_particles, n_subtrees, n_partitions,
      centroid_calculator, centroid_resumer,
      centroid_cache, this->thisProxy, subtree_opts
      );
    CkPrintf("Created %d Subtrees: %.3lf ms\n", n_subtrees,
        (CkWallTimer() - start_time) * 1000);

    // Create Partitions
    start_time = CkWallTimer();
    CkArrayOptions partition_opts(n_partitions);
    if (matching_decomps) partition_opts.bindTo(subtrees);
    else treespec.ckLocalBranch()->getPartitionDecomposition()->setArrayOpts(partition_opts);
    partitions = CProxy_Partition<CentroidData>::ckNew(
      n_partitions, centroid_cache, centroid_resumer,
      centroid_calculator, partition_opts
      );
    CkPrintf("Created %d Partitions: %.3lf ms\n", n_partitions,
        (CkWallTimer() - start_time) * 1000);

    // Flush decomposed particles to home Subtrees and Partitions
    // TODO Separate decomposition for Subtrees and Partitions
    start_time = CkWallTimer();
    readers.assignPartitions(universe.n_particles, n_partitions, partitions);
    CkStartQD(CkCallbackResumeThread());
    CkPrintf("Assigning particles to Partitions: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

    start_time = CkWallTimer();
    readers.flush(universe.n_particles, n_subtrees, subtrees);
    CkStartQD(CkCallbackResumeThread());
    CkPrintf("Flushing particles to Subtrees: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);
    CkPrintf("**Total Decomposition time: %.3lf ms\n",
        (CkWallTimer() - decomp_time) * 1000);
  }

  // Core iterative loop of the simulation
  void run(CkCallback cb) {
    auto config = treespec.ckLocalBranch()->getConfiguration();
    for (int iter = 0; iter < config.num_iterations; iter++) {
      CkPrintf("\n* Iteration %d\n", iter);

      // Start tree build in Subtrees
      start_time = CkWallTimer();
      CkCallback timeCb (CkReductionTarget(Driver<Data>, reportTime), this->thisProxy);
      subtrees.buildTree(partitions, timeCb);
      CkWaitQD();
      CkPrintf("Tree build and sending leaves: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // Prefetch into cache
      start_time = CkWallTimer();
      // use exactly one of these three commands to load the software cache
      paratreet::preTraversalFn(this->thisProxy, centroid_cache);
      CkWaitQD();
      CkPrintf("TreeCanopy cache loading: %.3lf ms\n",
          (CkWallTimer() - start_time) * 1000);

      // Perform traversals
      start_time = CkWallTimer();
      paratreet::traversalFn(universe, partitions, iter);
      CkWaitQD();
      CkPrintf("Tree traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // Call user's post-interaction function, which may for example:
      // Output particle accelerations for verification
      // TODO: Initial force interactions similar to ChaNGa
      paratreet::postInteractionsFn(universe, partitions, iter);

      // Move the particles in Partitions
      start_time = CkWallTimer();

      // Meta data collections
      CkReductionMsg * msg;
      subtrees.collectMetaData(updated_timestep_size, CkCallbackResumeThread((void *&) msg));
       // Parse Subtree reduction message
      int numRedn = 0;
      CkReduction::tupleElement* res = NULL;
      msg->toTuple(&res, &numRedn);
      max_velocity = *(Real*)(res[0].data); // avoid max_velocity = 0.0
      int maxParticlesSize = *(int*)(res[1].data);
      int sumParticlesSize = *(int*)(res[2].data);
      float avgTPSize = (float) sumParticlesSize / (float) n_subtrees;
      float ratio = (float) maxParticlesSize / avgTPSize;
      bool complete_rebuild = (config.flush_max_avg_ratio != 0?
          (ratio > config.flush_max_avg_ratio) : // use flush_max_avg_ratio when it is not 0
          (iter % config.flush_period == config.flush_period - 1)) ;
      CkPrintf("[Meta] n_subtree = %d; timestep_size = %f; maxSubtreeSize = %d; sumSubtreeSize = %d; avgSubtreeSize = %f; ratio = %f; maxVelocity = %f; rebuild = %s\n", n_subtrees, updated_timestep_size, maxParticlesSize,sumParticlesSize, avgTPSize, ratio, max_velocity, (complete_rebuild? "yes" : "no"));
      //End Subtree reduction message parsing

      Real max_universe_box_dimension = 0;
      for (int dim = 0; dim < NDIM; dim ++){
        Real length = universe.box.greater_corner[dim] - universe.box.lesser_corner[dim];
        if (length > max_universe_box_dimension)
          max_universe_box_dimension = length;
      }

      updated_timestep_size = max_universe_box_dimension / max_velocity / 100.0;
      if (updated_timestep_size > config.timestep_size) updated_timestep_size = config.timestep_size;
      partitions.perturb(subtrees, updated_timestep_size, complete_rebuild); // 0.1s for example
      CkWaitQD();
      CkPrintf("Perturbations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      if (iter % config.lb_period == config.lb_period - 1){
        start_time = CkWallTimer();
        subtrees.pauseForLB();
        partitions.pauseForLB();
        CkWaitQD();
        CkPrintf("Load balancing: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      }
      // Destroy subtrees and perform decomposition from scratch
      if (complete_rebuild) {
        treespec.reset();
        subtrees.destroy();
        partitions.destroy();
        decompose(iter+1);
      } else {
        partitions.reset();
        subtrees.reset();
      }

      // Clear cache and other storages used in this iteration
      centroid_cache.destroy(true);
#if COUNT_INTERACTIONS
      CkCallback statsCb (CkReductionTarget(Driver<Data>, countInts), this->thisProxy);
      centroid_resumer.collectAndResetStats(statsCb);
#endif
      storage.clear();
      storage_sorted = false;
      CkWaitQD();
    }

    cb.send();
  }

  // -------------------
  // Auxiliary functions
  // -------------------
  void reportTime(){
    CkPrintf("Tree build: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
  }

  void countInts(unsigned long long* intrn_counts) {
     CkPrintf("%llu node-particle interactions, %llu bucket-particle interactions %llu node opens, %llu node closes\n", intrn_counts[0], intrn_counts[1], intrn_counts[2], intrn_counts[3]);
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
