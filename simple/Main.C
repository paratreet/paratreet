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
/* readonly */ std::string input_file;
/* readonly */ int n_readers;
/* readonly */ int max_ppc; // particles per chare
/* readonly */ int max_ppl; // particles per leaf
/* readonly */ int tree_type;

class Main : public CBase_Main {
  double start_time;
  std::string tree_str;
  int n_treepieces;

  public:
    static void initialize() {
      BoundingBox::registerReducer();
    }

    Main(CkArgMsg* m) {
      mainProxy = thisProxy;

      // default values
      input_file = "";
      max_ppc = 128;
      max_ppl = 10;
      tree_type = OCT_TREE;

      // handle arguments
      int c;
      while ((c = getopt(m->argc, m->argv, "f:c:l:t:")) != -1) {
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
          case 't':
            tree_str = optarg;
            if (tree_str.compare("oct") == 0) {
              tree_type = OCT_TREE;
            }
            else if (tree_str.compare("sfc") == 0) {
              tree_type = SFC_TREE;
            }
            break;
          default:
            CkPrintf("Usage:\n");
            CkPrintf("\t-f [input file]\n");
            CkPrintf("\t-c [max # of particles per chare]\n");
            CkPrintf("\t-l [max # of particles per leaf]\n");
            CkPrintf("\t-t [tree type: oct, sfc]\n");
            CkExit();
        }
      }
      delete m;

      // print settings
      CkPrintf("\n[SIMPLE TREE]\n");
      CkPrintf("Input file: %s\n", input_file.c_str());
      CkPrintf("Max # of particles per chare: %d\n", max_ppc);
      CkPrintf("Max # of particles per leaf: %d\n", max_ppl);
      CkPrintf("Tree type: %s\n\n", (tree_type == OCT_TREE) ? "OCT" : "SFC");

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
      // call ParaTreeT decomposer
      int* result = NULL;
      decomposer.run(CkCallbackResumeThread((void*&)result));
      int n_treepieces = *result;

      // create TreePiece chare array
      treepieces = CProxy_TreePiece::ckNew(n_treepieces);

      // distribute particles to pieces
      decomposer.flush(CkCallbackResumeThread());

      // TODO build tree

      // all done
      terminate();
    }

    void terminate() {
      CkPrintf("\nElapsed time: %lf s\n", CkWallTimer() - start_time);
      CkExit();
    }
};

#include "simple.def.h"
