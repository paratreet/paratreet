#include "simple.decl.h"
#include "common.h"
#include "Reader.h"
#include "BoundingBox.h"

/* readonly */ CProxy_Main mainProxy;
/* readonly */ int initial_depth;
/* readonly */ int n_readers;

class Main : public CBase_Main {
  CProxy_Reader readers;
  double start_time;
  std::string input_file;

  public:
    static void initialize() {
      BoundingBox::registerReducer();
    }
    
    Main(CkArgMsg* m) {
      mainProxy = thisProxy;

      // default values
      initial_depth = 3;

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
      // record start time
      start_time = CkWallTimer();

      // load Tipsy data
      CkReductionMsg *result = NULL;
      readers.load(input_file, CkCallbackResumeThread((void*&)result));
    }

    void terminate() {
      CkPrintf("Elapsed time: %lf\n", CkWallTimer() - start_time);
      CkExit();
    }
};

#include "simple.def.h"
