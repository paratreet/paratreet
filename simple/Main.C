#include "simple.decl.h"
#include "common.h"
#include "Reader.h"
#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Splitter.h"

/* readonly */ CProxy_Main mainProxy;
/* readonly */ int initial_depth;
/* readonly */ int n_readers;
/* readonly */ int max_ppc; // particles per chare
/* readonly */ int max_ppl; // particles per leaf

class Main : public CBase_Main {
  CProxy_Reader readers;
  double start_time;
  std::string input_file;
  CkVec<Splitter> splitters;

  public:
    static void initialize() {
      BoundingBox::registerReducer();
    }
    
    Main(CkArgMsg* m) {
      mainProxy = thisProxy;

      // default values
      initial_depth = 3;
      max_ppc = 1024;
      max_ppl = 10;

      // handle arguments
      int c;
      while ((c = getopt(m->argc, m->argv, "d:f:")) != -1) {
        switch (c) {
          case 'd':
            initial_depth = atoi(optarg);
            break;
          case 'f':
            input_file = optarg;
            break;
          default:
            CkPrintf("Usage: %s -d [initial depth] -f [input file]\n", m->argv[0]);
            CkExit();
        }
      }
      delete m;

      // print settings
      CkPrintf("\nInitial Depth: %d\nInput file: %s\n\n", initial_depth, input_file.c_str());

      // create readers
      n_readers = CkNumNodes();
      readers = CProxy_Reader::ckNew();

      thisProxy.commence();
   }

    void commence() {
      CkReductionMsg* result = NULL;

      // record start time
      start_time = CkWallTimer();

      // load Tipsy data
      readers.load(input_file, CkCallbackResumeThread((void*&)result));

      // built universe
      BoundingBox universe = *((BoundingBox*)result->getData());
      delete result;

#ifdef DEBUG
      cout << "[Main] Universal bounding box: " << universe << endl;
#endif

      // assign keys and sort particles locally
      readers.assignKeys(universe, CkCallbackResumeThread());

      // create splitters
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
            splitters.push_back(Splitter(Splitter::convertKey(from), Splitter::convertKey(to), from, np)); // TODO
          }
        }

        keys.buffer();
        delete result;

        if (keys.size() == 0)
          break;
      }

      // send sorted splitters to readers
      splitters.quickSort();
      readers.setSplitters(splitters, CkCallbackResumeThread());
      splitters.resize(0);

      // TODO create chares and have readers send particles

      // all done
      terminate();
    }

    void terminate() {
      CkPrintf("\nElapsed time: %lf s\n", CkWallTimer() - start_time);
      CkExit();
    }
};

#include "simple.def.h"
