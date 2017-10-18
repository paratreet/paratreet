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
/* readonly */ int max_ppc; // particles per chare (treepiece)
/* readonly */ int max_ppl; // particles per leaf
/* readonly */ int decomp_type;
/* readonly */ int tree_type;

class Main : public CBase_Main {
  double start_time;
  std::string input_str;
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
      max_ppc = 10;
      max_ppl = 10;
      decomp_type = OCT_DECOMP;
      tree_type = OCT_TREE;

      // handle arguments
      int c;
      while ((c = getopt(m->argc, m->argv, "f:c:p:l:d:t:")) != -1) {
        switch (c) {
          case 'f':
            input_file = optarg;
            break;
          case 'c':
            n_chares = atoi(optarg);
            break;
          case 'p':
            max_ppc = atoi(optarg);
            break;
          case 'l':
            max_ppl = atoi(optarg);
            break;
          case 'd':
            input_str = optarg;
            if (input_str.compare("oct") == 0) {
              decomp_type = OCT_DECOMP;
            }
            else if (input_str.compare("sfc") == 0) {
              decomp_type = SFC_DECOMP;
            }
            break;
          case 't':
            input_str = optarg;
            if (input_str.compare("oct") == 0) {
              tree_type = OCT_TREE;
            }
            else if (input_str.compare("sfc") == 0) {
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
      CkPrintf("Decomposition type: %s\n", (decomp_type == OCT_DECOMP) ? "OCT" : "SFC");
      CkPrintf("Tree type: %s\n", (tree_type == OCT_TREE) ? "OCT" : "SFC");
      if (decomp_type == SFC_DECOMP) {
        CkPrintf("# of chares: %d\n", n_chares);
      }
      if (decomp_type == OCT_DECOMP) {
        CkPrintf("Max # of particles per chare: %d\n", max_ppc);
      }
      CkPrintf("Max # of particles per leaf: %d\n\n", max_ppl);

      // create Readers
      n_readers = CkNumNodes();
      readers = CProxy_Reader::ckNew();

      // create Decomposer
      decomposer = CProxy_Decomposer::ckNew();

      // create TreePieces or placeholder for them
      if (decomp_type == OCT_DECOMP) {
        treepieces = CProxy_TreePiece::ckNew();
        treepieces.doneInserting();
      }
      else if (decomp_type == SFC_DECOMP) {
        treepieces = CProxy_TreePiece::ckNew(n_chares);
      }

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
