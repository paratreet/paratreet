#ifndef PARATREET_DRIVER_H_
#define PARATREET_DRIVER_H_

#include "paratreet.decl.h"

#include "CoreFunctions.h"

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
#include "ThreadStateHolder.h"
#include "Modularization.h"
#include "Node.h"
#include "Writer.h"
#include "Subtree.h"

extern CProxy_Reader readers;
extern CProxy_TreeSpec treespec;
extern CProxy_ThreadStateHolder thread_state_holder;

template <typename Data>
class Driver : public CBase_Driver<Data> {
public:
  CProxy_TreeCanopy<Data> calculator;
  CProxy_CacheManager<Data> cache_manager;
  CProxy_Resumer<Data> resumer;
  std::vector<std::pair<Key, SpatialNode<Data>>> storage;
  bool storage_sorted;
  BoundingBox universe;
  CProxy_Subtree<Data> subtrees; // Cannot be a global readonly variable
  CProxy_Partition<Data> partitions;
  int n_subtrees;
  int n_partitions;
  double start_time;
  std::vector<int> partition_locations;

  Driver(CProxy_CacheManager<Data> cache_manager_, CProxy_Resumer<Data> resumer_, CProxy_TreeCanopy<Data> calculator_) :
    cache_manager(cache_manager_), resumer(resumer_), calculator(calculator_), storage_sorted(false) {}

  // Performs initial decomposition
  void init(const CkCallback& cb, const paratreet::Configuration& cfg) {
    // Ensure all treespecs have been created
    CkPrintf("* Validating tree specifications.\n");
    treespec.receiveConfiguration(
      CkCallbackResumeThread(),
      const_cast<paratreet::Configuration*>(&cfg)
    );
    // Then, initialize the cache managers
    CkPrintf("* Initializing cache managers.\n");
    cache_manager.initialize(CkCallbackResumeThread());
    // Useful particle keys
    CkPrintf("* Initialization\n");
    decompose(0);
    cb.send();
  }

  void remakeUniverse() {
    Vector3D<Real> bsize = universe.box.size();
    Real max = (bsize.x > bsize.y) ? bsize.x : bsize.y;
    max = (max > bsize.z) ? max : bsize.z;
    Vector3D<Real> bcenter = universe.box.center();
    // The magic number below is approximately 2^(-19)
    const Real fEps = 1.0 + 1.91e-6;  // slop to ensure keys fall between 0 and 1.
    bsize = Vector3D<Real>(fEps*0.5*max);
    universe.box = OrientedBox<Real>(bcenter-bsize, bcenter+bsize);
    thread_state_holder.setUniverse(universe);

    std::cout << "Universal bounding box: " << universe << " with volume "
      << universe.box.volume() << std::endl;
  }

  void partitionLocation(int partition_idx, int home_pe) {
     partition_locations[partition_idx] = home_pe;
  }

  // Performs decomposition by distributing particles among Subtrees,
  // by either loading particle information from input file or re-computing
  // the universal bounding box
  void decompose(int iter) {
    auto& config = paratreet::getConfiguration();
    double decomp_time = CkWallTimer();
    if (iter == 0) {
      // Build universe
      start_time = CkWallTimer();
      CkReductionMsg* result;
      readers.load(config.input_file, CkCallbackResumeThread((void*&)result));
      CkPrintf("Loading Tipsy data and building universe: %.3lf ms\n",
          (CkWallTimer() - start_time) * 1000);
      universe = *((BoundingBox*)result->getData());
      delete result;
      remakeUniverse();
      if (config.min_n_subtrees < CkNumPes() || config.min_n_partitions < CkNumPes()) {
        CkPrintf("WARNING: Consider increasing min_n_subtrees and min_n_partitions to at least #pes\n");
      }
      // Assign keys and sort particles locally
      start_time = CkWallTimer();
      readers.assignKeys(universe, CkCallbackResumeThread());
      CkPrintf("Assigning keys and sorting particles: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);
    } else CkWaitQD();

    bool matching_decomps = config.decomp_type == paratreet::subtreeDecompForTree(config.tree_type);
    // Set up splitters for decomposition
    start_time = CkWallTimer();
    n_partitions = treespec.ckLocalBranch()->getPartitionDecomposition()->findSplitters(universe, readers, config.min_n_partitions);
    partition_locations.resize(n_partitions);
    treespec.receiveDecomposition(CkCallbackResumeThread(),
        CkPointer<Decomposition>(treespec.ckLocalBranch()->getPartitionDecomposition()), false);
    CkPrintf("Setting up splitters for particle decompositions: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

    // Create Partitions
    CkArrayOptions partition_opts(n_partitions);
    treespec.ckLocalBranch()->getPartitionDecomposition()->setArrayOpts(partition_opts, {}, false);
    partitions = CProxy_Partition<Data>::ckNew(
      n_partitions, cache_manager, resumer, calculator,
      this->thisProxy, matching_decomps, partition_opts
      );
    CkPrintf("Created %d Partitions: %.3lf ms\n", n_partitions,
        (CkWallTimer() - start_time) * 1000);

    start_time = CkWallTimer();
    readers.assignPartitions(n_partitions, partitions);
    CkStartQD(CkCallbackResumeThread());
    CkPrintf("Assigning particles to Partitions: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);

    start_time = CkWallTimer();
    if (matching_decomps) {
      n_subtrees = n_partitions;
      CkPrintf("Using same decomposition for subtrees and partitions\n");
      treespec.receiveDecomposition(CkCallbackResumeThread(),
        CkPointer<Decomposition>(treespec.ckLocalBranch()->getPartitionDecomposition()), true);
    }
    else {
      n_subtrees = treespec.ckLocalBranch()->getSubtreeDecomposition()->findSplitters(universe, readers, config.min_n_subtrees);
      treespec.receiveDecomposition(CkCallbackResumeThread(),
        CkPointer<Decomposition>(treespec.ckLocalBranch()->getSubtreeDecomposition()), true);
      CkPrintf("Setting up splitters for subtree decompositions: %.3lf ms\n",
          (CkWallTimer() - start_time) * 1000);
    }

    // Create Subtrees
    start_time = CkWallTimer();
    CkArrayOptions subtree_opts(n_subtrees);
    if (matching_decomps) subtree_opts.bindTo(partitions);
    treespec.ckLocalBranch()->getSubtreeDecomposition()->setArrayOpts(subtree_opts, partition_locations, !matching_decomps);
    subtrees = CProxy_Subtree<Data>::ckNew(
      CkCallbackResumeThread(),
      universe.n_particles, n_subtrees, n_partitions,
      calculator, resumer,
      cache_manager, this->thisProxy, matching_decomps, subtree_opts
      );
    CkPrintf("Created %d Subtrees: %.3lf ms\n", n_subtrees,
        (CkWallTimer() - start_time) * 1000);

    start_time = CkWallTimer();
    readers.flush(n_subtrees, subtrees);
    CkStartQD(CkCallbackResumeThread());
    CkPrintf("Flushing particles to Subtrees: %.3lf ms\n",
        (CkWallTimer() - start_time) * 1000);
    CkPrintf("**Total Decomposition time: %.3lf ms\n",
        (CkWallTimer() - decomp_time) * 1000);
  }

  // Core iterative loop of the simulation
  void run(CkCallback cb) {
    auto& config = paratreet::getConfiguration();
    double total_time = 0;
    for (int iter = 0; iter < config.num_iterations; iter++) {
      CkPrintf("\n* Iteration %d\n", iter);
      double iter_start_time = CkWallTimer();
      // Start tree build in Subtrees
      start_time = CkWallTimer();
      CkCallback timeCb (CkReductionTarget(Driver<Data>, reportTime), this->thisProxy);
      subtrees.buildTree(partitions, timeCb);
      CkWaitQD();
      CkPrintf("Tree build and sending leaves: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      // Meta data collections, first for max velo
      CkReductionMsg * msg, *msg2;
      subtrees.collectMetaData(CkCallbackResumeThread((void *&) msg));
      // Parse Subtree reduction message
      int numRedn = 0, numRedn2 = 0;
      CkReduction::tupleElement* res = nullptr, *res2 = nullptr;
      msg->toTuple(&res, &numRedn);
      Real max_velocity = *(Real*)(res[0].data); // avoid max_velocity = 0.0
      Real timestep_size = paratreet::getTimestep(universe, max_velocity);

      ProxyPack<Data> proxy_pack (this->thisProxy, subtrees, partitions, cache_manager);

      // Prefetch into cache
      start_time = CkWallTimer();
      // use exactly one of these three commands to load the software cache
      paratreet::preTraversalFn(proxy_pack);
      CkWaitQD();
      CkPrintf("TreeCanopy cache loading: %.3lf ms\n",
          (CkWallTimer() - start_time) * 1000);

      // Perform traversals
      start_time = CkWallTimer();
      paratreet::traversalFn(universe, proxy_pack, iter);
      CkWaitQD();
      CkPrintf("Tree traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);

      start_time = CkWallTimer();

      // Move the particles in Partitions
      partitions.kick(timestep_size, CkCallbackResumeThread());

      // Now track PE imbalance for memory reasons
      thread_state_holder.collectMetaData(CkCallbackResumeThread((void *&) msg2));
      msg2->toTuple(&res2, &numRedn2);
      int maxPESize = *(int*)(res2[0].data);
      int sumPESize = *(int*)(res2[1].data);
      float avgPESize = (float) universe.n_particles / (float) CkNumPes();
      float ratio = (float) maxPESize / avgPESize;
      bool complete_rebuild = (config.flush_period == 0) ?
          (ratio > config.flush_max_avg_ratio) :
          (iter % config.flush_period == config.flush_period - 1) ;
      CkPrintf("[Meta] n_subtree = %d; timestep_size = %f; sumPESize = %d; maxPESize = %d, avgPESize = %f; ratio = %f; maxVelocity = %f; rebuild = %s\n", n_subtrees, timestep_size, sumPESize, maxPESize, avgPESize, ratio, max_velocity, (complete_rebuild? "yes" : "no"));
      //End Subtree reduction message parsing

      paratreet::postIterationFn(universe, proxy_pack, iter);

      CkReductionMsg* result;
      partitions.perturb(timestep_size, CkCallbackResumeThread((void *&)result));
      universe = *((BoundingBox*)result->getData());
      delete result;
      remakeUniverse();
      partitions.rebuild(universe, subtrees, complete_rebuild); // 0.1s for example
      CkWaitQD();
      CkPrintf("Perturbations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      if (!complete_rebuild && iter % config.lb_period == config.lb_period - 1){
        start_time = CkWallTimer();
        //subtrees.pauseForLB(); // move them later
        partitions.pauseForLB();
        CkWaitQD();
        CkPrintf("Load balancing: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      }
      // Destroy subtrees and perform decomposition from scratch
      resumer.reset();
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
      cache_manager.destroy(true);
      CkCallback statsCb (CkReductionTarget(Driver<Data>, countInts), this->thisProxy);
      thread_state_holder.collectAndResetStats(statsCb);
      storage.clear();
      storage_sorted = false;
      CkWaitQD();
      double iter_time = CkWallTimer() - iter_start_time;
      total_time += iter_time;
      CkPrintf("Iteration %d time: %.3lf ms\n", iter, iter_time * 1000);
      if (iter == config.num_iterations-1) {
        CkPrintf("Average iteration time: %.3lf ms\n", total_time / config.num_iterations * 1000);
      }
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
    auto& config = paratreet::getConfiguration();
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
