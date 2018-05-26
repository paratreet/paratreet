#include "OctTree.h"

#include <vector>

#include "Splitter.h"
#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Utility.h"
#include "Reader.h"
#include "TreePiece.h"
#include "TreeElement.h"
#include "templates.h"

std::vector<Splitter> findSplitters(CProxy_Reader readers, BoundingBox& universe,
                                    int max_particles_per_tp);


OctTree::OctTree(int max_particles_per_leaf, int max_particles_per_tp, Real expand_ratio,
                 CProxy_Reader& readers) :
    max_particles_per_leaf(max_particles_per_leaf),
    max_particles_per_tp(max_particles_per_tp),
    expand_ratio(expand_ratio),
    readers(readers)
{
    build();
}


CProxy_TreePiece<CentroidData> OctTree::treepieces_proxy() {
    return treepieces;
}


void OctTree::redistribute() {
    CkReductionMsg* result;
    double t;

    treepieces.assignKeys(CkCallbackResumeThread((void*&)result));
    bool is_all_particles_in_universe = *reinterpret_cast<bool*>(result->getData());
    delete result;    
    if (is_all_particles_in_universe) {
        CkPrintf("[OctTree] All particles in universe. Reusing treepieces...\n");

        t = CkWallTimer();
        treepieces.redistribute();
        CkWaitQD();
        CkPrintf("[OctTree] Redistributed particles in %lfs\n", CkWallTimer()-t);

        t = CkWallTimer();        
        treepieces.rebuild(CkCallbackResumeThread());
        CkPrintf("[OctTree] Rebuilt local trees in %lfs\n", CkWallTimer()-t);        
    } else {
        CkPrintf("[OctTree] Some particles out of universe. Recreating treepieces...\n");        
        destroy();
        build();
    }

    treepieces.checkParticlesChanged(CkCallbackResumeThread((void*&)result));
    bool is_particles_changed = *reinterpret_cast<bool*>(result->getData());
    delete result;
    if (is_particles_changed) {
        CkPrintf("[OctTree] Particles are changed\n");
    } else {
        CkPrintf("[OctTree] Particles are NOT changed\n");
    }

    treepieces.computeParticleNum(CkCallbackResumeThread((void*&)result));
    CkPrintf("[OctTree] n_particles=%d\n", *reinterpret_cast<int*>(result->getData()));
    delete result;
}


void OctTree::build() {
    CkPrintf("[OctTree] Start building oct tree...\n");

    CkReductionMsg* result;
    double t;

    // Compute bounding box
    t = CkWallTimer();
    readers.computeUniverseBoundingBox(CkCallbackResumeThread((void*&)result));
    BoundingBox universe = *((BoundingBox*)result->getData());
    delete result;
    CkPrintf("[OctTree] Computed universe bounding box in %lfs\n", CkWallTimer()-t);

    // Expand bounding box to reduce the probability of particles moving out of it
    universe.expand(expand_ratio);
    
    // Assign keys
    t = CkWallTimer();
    readers.assignKeys(universe, CkCallbackResumeThread());
    CkPrintf("[OctTree] Assigned keys in %lfs\n", CkWallTimer()-t);

    // Find splitters
    t = CkWallTimer();    
    const std::vector<Splitter>& splitters = findSplitters(
        readers, universe, max_particles_per_tp
    );
    readers.setSplitters(splitters, CkCallbackResumeThread()); // needed by TreePiece constructor    
    CkPrintf("[OctTree] Found splitters in %lfs\n", CkWallTimer()-t);
    
    // Create treepieces
    t = CkWallTimer();
    int n_treepieces = splitters.size();
    treepieces = CProxy_TreePiece<CentroidData>::ckNew(
        CkCallbackResumeThread(), universe.n_particles, n_treepieces, universe, n_treepieces
    );
    CkPrintf("[OctTree] Created %d tree pieces in %lfs\n", n_treepieces, CkWallTimer()-t);

    // Flush particles to treepieces
    t = CkWallTimer();
    readers.flush(universe.n_particles, n_treepieces, treepieces);
    CkWaitQD();
    CkPrintf("[OctTree] Flushed particles to treepieces in %lfs\n", CkWallTimer()-t);

    // Build local tree in TreePieces
    t = CkWallTimer();    
    treepieces.build(CkCallbackResumeThread());
    CkPrintf("[OctTree] Built local trees in %lfs\n", CkWallTimer()-t);    


}


void OctTree::destroy() {
    // Flush particles to readers
    treepieces.flush(readers);
    CkWaitQD();

    treepieces.ckDestroy();
}


std::vector<Splitter> findSplitters(CProxy_Reader readers, BoundingBox& universe,
                                    int max_particles_per_tp)
{
    BufferedVec<Key> keys;
    std::vector<Splitter> splitters;

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

    // TODO return as an error
    if (decomp_particle_sum != universe.n_particles) {
      CkPrintf("[Main] ERROR! Only %d particles out of %d decomposed\n",
                decomp_particle_sum, universe.n_particles);
      CkAbort("Decomposition error");
    }

    std::sort(splitters.begin(), splitters.end());    
    return splitters;
}
