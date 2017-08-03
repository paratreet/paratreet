#include "simple.decl.h"
#include "common.h"

#include "Reader.h"
#include "Decomposer.h"
#include "TreePiece.h"

#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Splitter.h"

/* readonly */ CProxy_Main mainProxy;
/* readonly */ CProxy_Reader readers;
/* readonly */ CProxy_Decomposer decomposer;
/* readonly */ CProxy_TreePiece treepieces;
/* readonly */ int n_readers;
/* readonly */ int max_ppc; // particles per chare
/* readonly */ int max_ppl; // particles per leaf

class Main : public CBase_Main {
  double start_time;
  std::string input_file;
  int n_treepieces;

  public:
    static void initialize() {
      BoundingBox::registerReducer();
    }

    Main(CkArgMsg* m) {
      mainProxy = thisProxy;

      // default values
      max_ppc = 128;
      max_ppl = 10;

      // handle arguments
      int c;
      while ((c = getopt(m->argc, m->argv, "f:c:l:")) != -1) {
        switch (c) {
          case 'f':
            input_file = optarg;
            break;
          case 'c':
            max_ppc = atoi(optarg);
            break;
          case 'l':
            max_ppl = atoi(optarg);
            break;
          default:
            CkPrintf("Usage: %s -f [input file] -c [max # of particles per chare] -l [max # of particles per leaf]\n", m->argv[0]);
            CkExit();
        }
      }
      delete m;

      // print settings
      CkPrintf("\n[SIMPLE TREE]\nInput file: %s\nMax # of particles per chare: %d\nMax # of particles per leaf: %d\n\n", input_file.c_str(), max_ppc, max_ppl);

      // create readers
      n_readers = CkNumNodes();
      readers = CProxy_Reader::ckNew();

      // create decomposer
      decomposer = CProxy_Decomposer::ckNew();

      // start!
      start_time = CkWallTimer();
      thisProxy.commence();
   }

    void commence() {
      int* result = NULL;
      decomposer.run(input_file, CkCallbackResumeThread((void*&)result));
      int n_treepieces = *result;

      // create TreePiece chare array
      treepieces = CProxy_TreePiece::ckNew(n_treepieces);

      // TODO have readers distribute particles to pieces

      // all done
      terminate();
    }

    void terminate() {
      CkPrintf("\nElapsed time: %lf s\n", CkWallTimer() - start_time);
      CkExit();
    }
};

#include "simple.def.h"
