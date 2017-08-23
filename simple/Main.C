#include "simple.decl.h"
#include "common.h"

#include "Reader.h"
#include "Decomposer.h"
#include "TreePiece.h"

#include "BoundingBox.h"
#include "BufferedVec.h"

/* readonly */ CProxy_Main mainProxy;
/* readonly */ CProxy_Reader readers;
/* readonly */ CProxy_Decomposer decomposer;
/* readonly */ CProxy_TreePiece treepieces;
/* readonly */ std::string input_file;
/* readonly */ int n_readers;
/* readonly */ int n_chares;
/* readonly */ double decomp_tolerance;
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
      n_chares = 128;
      decomp_tolerance = 0.1;
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
            n_chares = atoi(optarg);
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
            CkPrintf("\t-c [# of chares]\n");
            CkPrintf("\t-l [max # of particles per leaf]\n");
            CkPrintf("\t-t [tree type: oct, sfc]\n");
            CkExit();
        }
      }
      delete m;

      // print settings
      CkPrintf("\n[SIMPLE TREE]\n");
      CkPrintf("Input file: %s\n", input_file.c_str());
      CkPrintf("# of chares: %d\n", n_chares);
      CkPrintf("Max # of particles per leaf: %d\n", max_ppl);
      CkPrintf("Tree type: %s\n\n", (tree_type == OCT_TREE) ? "OCT" : "SFC");

      // create Readers
      n_readers = CkNumNodes();
      readers = CProxy_Reader::ckNew();

      // create Decomposer
      decomposer = CProxy_Decomposer::ckNew();

      // create TreePieces
      treepieces = CProxy_TreePiece::ckNew(n_chares);

      // start!
      start_time = CkWallTimer();
      decomposer.run();
   }

    void terminate() {
      CkPrintf("\nElapsed time: %lf s\n", CkWallTimer() - start_time);
      CkExit();
    }
};

#include "simple.def.h"
