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

  Driver(CProxy_CacheManager<Data> cache_manageri) : cache_manager(cache_manageri) {}
  void recvTE(std::pair<Key, Data> param) {
    storage.emplace_back(param);
  }
  void loadCache(int num_share_levels, CkCallback cb) {
    Comparator<Data> comp;
    std::sort(storage.begin(), storage.end(), comp);
    int send_size = storage.size();
    if (num_share_levels >= 0) {
      std::pair<Key, Data> to_search (1 << (LOG_BRANCH_FACTOR * num_share_levels), Data());
      send_size = std::lower_bound(storage.begin(), storage.end(), to_search, comp) - storage.begin();
    }
    cache_manager.recvStarterPack(storage.data(), send_size, cb);
  }

  void load(CkCallback cb) {
    total_start_time = CkWallTimer();
    // useful particle keys
    smallest_particle_key = Utility::removeLeadingZeros(Key(1));
    largest_particle_key = (~Key(0));

    // load Tipsy data and build universe
    start_time = CkWallTimer();
    CkReductionMsg* result;
    readers.load(input_file, CkCallbackResumeThread((void*&)result));
    CkPrintf("[Driver] Loading Tipsy data and building universe: %lf seconds\n", CkWallTimer() - start_time);

    universe = *((BoundingBox*)result->getData());
    delete result;

#ifdef DEBUG
    std::cout << "[Driver] Universal bounding box: " << universe << " with volume " << universe.box.volume() << std::endl;
#endif

    // assign keys and sort particles locally
    start_time = CkWallTimer();
    readers.assignKeys(universe, CkCallbackResumeThread());
    CkPrintf("[Driver] Assigning keys and sorting particles: %lf seconds\n", CkWallTimer() - start_time);

    start_time = CkWallTimer();
    findOctSplitters();
    std::sort(splitters.begin(), splitters.end());
    CkPrintf("[Driver] Finding and sorting splitters: %lf seconds\n", CkWallTimer() - start_time);
    readers.setSplitters(splitters, CkCallbackResumeThread());
    
    // create treepieces
    treepieces = CProxy_TreePiece<CentroidData>::ckNew(CkCallbackResumeThread(), universe.n_particles, n_treepieces, centroid_calculator, centroid_resumer, centroid_cache, centroid_driver, n_treepieces);
    CkPrintf("[Driver] Created %d TreePieces\n", n_treepieces);

    // flush particles to home TreePieces
    start_time = CkWallTimer();
    readers.flush(universe.n_particles, n_treepieces, treepieces);
    CkStartQD(CkCallbackResumeThread());
    CkPrintf("[Driver] Flushing particles to TreePieces: %lf seconds\n", CkWallTimer() - start_time);

#ifdef DEBUG
    // check if all treepieces have received the right number of particles
    treepieces.check(CkCallbackResumeThread());
#endif

    // free splitter memory
    splitters.resize(0);

    // start local tree build in TreePieces
    start_time = CkWallTimer();
    treepieces.build(true);
    CkWaitQD();
    CkPrintf("[Driver] Local tree build: %lf seconds\n", CkWallTimer() - start_time);
    centroid_driver.loadCache(num_share_levels, CkCallbackResumeThread());

    cb.send();
  }

  void run(CkCallback cb) {
    // perform downward and upward traversals (Barnes-Hut)
    start_time = CkWallTimer();
    treepieces.template startDown<GravityVisitor>();
    CkWaitQD();
#ifdef DELAYLOCAL
    treepieces.processLocal(CkCallbackResumeThread());
#endif
    CkPrintf("[Driver] Downward traversal done: %lf seconds\n", CkWallTimer() - start_time);
    start_time = CkWallTimer();
    treepieces.interact(CkCallbackResumeThread());
    CkPrintf("[Driver] Interactions done: %lf seconds\n", CkWallTimer() - start_time);
    //count_manager.sum(CkCallback(CkReductionTarget(Main, terminate), thisProxy));
    cb.send();
  }

private:
  double total_start_time;
  double start_time;
  int num_share_levels;
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
