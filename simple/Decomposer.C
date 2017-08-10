#include "Decomposer.h"
#include "BufferedVec.h"

extern CProxy_Reader readers;
extern std::string input_file;
extern int max_ppc;
extern int tree_type;

Decomposer::Decomposer() {}

void Decomposer::run(const CkCallback& cb) {
  CkReductionMsg* result = NULL;

  // load Tipsy data and build universe
  start_time = CkWallTimer();
  readers.load(input_file, CkCallbackResumeThread((void*&)result));
  CkPrintf("[Decomposer] Loading Tipsy data and building universe: %lf seconds\n", CkWallTimer() - start_time);
  
  BoundingBox universe = *((BoundingBox*)result->getData());
  delete result;

#ifdef DEBUG
  cout << "[Decomposer] Universal bounding box: " << universe << endl;
#endif

  // assign keys and sort particles locally
  start_time = CkWallTimer();
  readers.assignKeys(universe, CkCallbackResumeThread());
  CkPrintf("[Decomposer] Assigning keys and sorting particles: %lf seconds\n", CkWallTimer() - start_time);

  // create splitters
  start_time = CkWallTimer();
  createSplitters();
  CkPrintf("[Decomposer] Creating splitters: %lf seconds\n", CkWallTimer() - start_time);

#ifdef DEBUG
  cout << "[Decomposer] Number of splitters: " << n_splitters << endl;
#endif

  // back to callee
  cb.send(&n_splitters);
}

void Decomposer::createSplitters() {
  CkReductionMsg* result = NULL;
  int num_splitters;

  BufferedVec<Key> keys;
  keys.add(Key(1));
  keys.add(~Key(0));
  keys.buffer();

  while (1) {
    readers.count(keys.get(), CkCallbackResumeThread((void*&)result));
    int* counts = (int*)result->getData();
    int n_counts = result->getSize() / sizeof(int);

    CkAssert(2 * n_counts == keys.size());

    Real threshold = (Real)max_ppc * TOLERANCE;
    for (int i = 0; i < n_counts; i++) {
      Key from = keys.get(2*i);
      Key to = keys.get(2*i+1);

      int np = counts[i];
      if ((Real)np > threshold) {
        // add keys to halve the space
        keys.add(from << 1);
        keys.add((from << 1)+1);
        keys.add((from << 1)+1);

        if (to == (~Key(0))) {
          keys.add(~Key(0));
        }
        else {
          keys.add(to << 1);
        }
      }
      else {
        splitters.push_back(Splitter(Splitter::convertKey(from), Splitter::convertKey(to), from, np));
      }
    }

    keys.buffer();
    delete result;

    if (keys.size() == 0)
      break;
  }

  // sort splitters and send them to readers
  splitters.quickSort();
  readers.setSplitters(splitters, CkCallbackResumeThread());
  n_splitters = splitters.size();
  splitters.resize(0);
}
