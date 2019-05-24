#ifndef SIMPLE_DRIVER_H_
#define SIMPLE_DRIVER_H_

#include "simple.decl.h"
#include "common.h"
#include <algorithm>
#include <vector>

#include <numeric>
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

extern CProxy_Reader readers;
extern int n_readers;
extern double decomp_tolerance;
extern int max_particles_per_tp; // for OCT decomposition
extern int max_particles_per_leaf; // for local tree build
extern int decomp_type;
extern int tree_type;
extern int num_iterations;
extern int flush_period;
extern CProxy_TreeElement<CentroidData> centroid_calculator;
extern CProxy_CacheManager<CentroidData> centroid_cache;
extern CProxy_Resumer<CentroidData> centroid_resumer;
extern CProxy_CountManager count_manager;
extern CProxy_Driver<CentroidData> centroid_driver;

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

  Driver(CProxy_CacheManager<Data> cache_manageri) : cache_manager(cache_manageri), storage_sorted(false) {}

  void countInts(int* intrn_counts) {
    CkPrintf("%d node-part interactions, %d part-part interactions\n", intrn_counts[0], intrn_counts[1] / 2);
  }

  void recvTE(std::pair<Key, Data> param) {
    storage.emplace_back(param);
  }
  void loadCache(CkCallback cb) {
    CkPrintf("Broadcasting top %d levels to caches\n", num_share_levels);
    Comparator<Data> comp;
    std::sort(storage.begin(), storage.end(), comp);
    int send_size = storage.size();
    if (num_share_levels >= 0) {
      std::pair<Key, Data> to_search (1 << (LOG_BRANCH_FACTOR * num_share_levels), Data());
      send_size = std::lower_bound(storage.begin(), storage.end(), to_search, comp) - storage.begin();
    }
    cache_manager.recvStarterPack(storage.data(), send_size, cb);
  }

  void load(Config config, CkCallback cb) {
    total_start_time = CkWallTimer();
    makeNewTree(0);
    cb.send();
  }
  void makeNewTree(int it) {
    // useful particle keys
    smallest_particle_key = Utility::removeLeadingZeros(Key(1));
    largest_particle_key = (~Key(0));

    // load Tipsy data and build universe
    start_time = CkWallTimer();
    CkReductionMsg* result;
    if (it == 0) {
      readers.load(input_file, CkCallbackResumeThread((void*&)result));
      CkPrintf("[Driver, %d] Loading Tipsy data and building universe: %lf seconds\n", it, CkWallTimer() - start_time);
    }
    else {
      readers.computeUniverseBoundingBox(CkCallbackResumeThread((void*&)result));
      CkPrintf("[Driver, %d] Rebuilding universe: %lf seconds\n", it, CkWallTimer() - start_time);
    }
    universe = *((BoundingBox*)result->getData());
    delete result;

#ifdef DEBUG
    std::cout << "[Driver] Universal bounding box: " << universe << " with volume " << universe.box.volume() << std::endl;
#endif

    // assign keys and sort particles locally
    start_time = CkWallTimer();
    readers.assignKeys(universe, CkCallbackResumeThread());
    CkPrintf("[Driver, %d] Assigning keys and sorting particles: %lf seconds\n", it, CkWallTimer() - start_time);

    start_time = CkWallTimer();
    findOctSplitters();
    std::sort(splitters.begin(), splitters.end());
    CkPrintf("[Driver, %d] Finding and sorting splitters: %lf seconds\n", it, CkWallTimer() - start_time);
    readers.setSplitters(splitters, CkCallbackResumeThread());
    
    // create treepieces
    CkWaitQD();
    treepieces = CProxy_TreePiece<CentroidData>::ckNew(CkCallbackResumeThread(), universe.n_particles, n_treepieces, centroid_calculator, centroid_resumer, centroid_cache, centroid_driver, n_treepieces);
    CkWaitQD();
    CkPrintf("[Driver, %d] Created %d TreePieces\n", it, n_treepieces);

    // flush particles to home TreePieces
    start_time = CkWallTimer();
    readers.flush(universe.n_particles, n_treepieces, treepieces);
    CkStartQD(CkCallbackResumeThread());
    CkPrintf("[Driver, %d] Flushing particles to TreePieces: %lf seconds\n", it, CkWallTimer() - start_time);

#ifdef DEBUG
    // check if all treepieces have received the right number of particles
    treepieces.check(CkCallbackResumeThread());
#endif

    // free splitter memory
    splitters.resize(0);

  }

  void sortStorage() {
    Comparator<Data> comp;
    std::sort(storage.begin(), storage.end(), comp);
    storage_sorted = true;
  }

  template <typename Visitor>
  void prefetch(Data nodewide_data, int cm_index, TEHolder<Data> te_holder, CkCallback cb) {
    // do traversal on the root, send everything
    if (!storage_sorted) sortStorage();
    std::queue<int> node_indices; // better for cache. plus no requirement here on order
    node_indices.push(0);
    std::vector<std::pair<Key, Data>> to_send;
    Visitor v;
    Comparator<Data> comp;
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
          it = std::lower_bound(storage.begin(), storage.end(), std::make_pair(key, Data()), comp);
          if (it != storage.end() && it->first == key) {
            node_indices.push(it - storage.begin());
          }
        }
      }
    }
    cache_manager[cm_index].recvStarterPack(to_send.data(), to_send.size(), cb);
  }
  void request(Key* request_list, int list_size, int cm_index, TEHolder<Data> te_holder, CkCallback cb) {
    if (!storage_sorted) sortStorage();
    Comparator<Data> comp;
    typename std::vector<std::pair<Key, Data> >::iterator it;
    std::vector<std::pair<Key, Data>> to_send;
    for (int i = 0; i < list_size; i++) {
      Key key = request_list[i];
      it = std::lower_bound(storage.begin(), storage.end(), std::make_pair(key, Data()), comp);
      if (it != storage.end() && it->first == key) {
        to_send.push_back(*it);
      }
    }
    cache_manager[cm_index].recvStarterPack(to_send.data(), to_send.size(), cb);
  }

  void run(CkCallback cb, int num_iterations) {
    for (int it = 0; it < num_iterations; it++) {
      // start local tree build in TreePieces
      start_time = CkWallTimer();
      treepieces.build(true);
      CkWaitQD();
      CkPrintf("[Driver, %d] Local tree build: %lf seconds\n", it, CkWallTimer() - start_time);
      start_time = CkWallTimer();
      centroid_cache.template startPrefetch<GravityVisitor>(this->thisProxy, centroid_calculator, CkCallback::ignore);
      //centroid_driver.loadCache(CkCallbackResumeThread());
      CkWaitQD();
      CkPrintf("[Driver, %d] TE cache loading: %lf seconds\n", it, CkWallTimer() - start_time);

      // perform downward and upward traversals (Barnes-Hut)
      start_time = CkWallTimer();
      treepieces.template startDown<GravityVisitor>();
      //treepieces.template startUpAndDown<DensityVisitor>();
      CkWaitQD();
#if DELAYLOCAL
      //treepieces.processLocal(CkCallbackResumeThread());
#endif
      CkPrintf("[Driver, %d] Traversal done: %lf seconds\n", it, CkWallTimer() - start_time);
      start_time = CkWallTimer();
      treepieces.interact(CkCallbackResumeThread());
      CkPrintf("[Driver, %d] Interactions done: %lf seconds\n", it, CkWallTimer() - start_time);
      //count_manager.sum(CkCallback(CkReductionTarget(Main, terminate), thisProxy));
      start_time = CkWallTimer();
      bool complete_rebuild = (it % flush_period == flush_period-1);
      treepieces.perturb(0.1, complete_rebuild); // 0.1s for example
      CkWaitQD();
      CkPrintf("[Driver, %d] Perturbations done: %lf seconds\n", it, CkWallTimer() - start_time);
      if (complete_rebuild) {
        treepieces.ckDestroy();
        makeNewTree(it+1);
      }
      centroid_cache.destroy(true);
      centroid_resumer.destroy();
      storage.resize(0);
      storage_sorted = false;
      CkWaitQD();
    }
    cb.send();
  }

private:
  double total_start_time;
  double start_time;
  BoundingBox universe;
  Key smallest_particle_key;
  Key largest_particle_key;

  std::vector<Splitter> splitters;

  CProxy_TreePiece<CentroidData> treepieces; // cannot be a global variable
  int n_treepieces;

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
      CkPrintf("[Driver] ERROR! Only %d particles out of %d decomposed\n",
          decomp_particle_sum, universe.n_particles);
      CkAbort("Decomposition error");
    }

    // determine number of TreePieces
    // override input from user if there was one
    n_treepieces = splitters.size();
  }
};

#endif // SIMPLE_DRIVER_H_
