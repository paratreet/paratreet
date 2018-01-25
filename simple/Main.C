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
/* readonly */ std::string input_file;
/* readonly */ int n_readers;
/* readonly */ double decomp_tolerance;
/* readonly */ int max_particles_per_tp; // for OCT decomposition
/* readonly */ int max_particles_per_leaf; // for local tree build
/* readonly */ int decomp_type;
/* readonly */ int tree_type;

class Main : public CBase_Main {
  double start_time;
  std::string input_str;

  public:
    static void initialize() {
      BoundingBox::registerReducer();
    }

    Main(CkArgMsg* m) {
      mainProxy = thisProxy;

      // default values
      int n_treepieces = 0; // cannot be a readonly because of OCT decomposition
      input_file = "";
      decomp_tolerance = 0.1;
      max_particles_per_tp = 10;
      max_particles_per_leaf = 1;
      decomp_type = OCT_DECOMP;
      tree_type = OCT_TREE;

      // handle arguments
      int c;
      while ((c = getopt(m->argc, m->argv, "f:n:p:l:d:t:")) != -1) {
        switch (c) {
          case 'f':
            input_file = optarg;
            break;
          case 'n':
            n_treepieces = atoi(optarg);
            break;
          case 'p':
            max_particles_per_tp = atoi(optarg);
            break;
          case 'l':
            max_particles_per_leaf = atoi(optarg);
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
            break;
          default:
            CkPrintf("Usage:\n");
            CkPrintf("\t-f [input file]\n");
            CkPrintf("\t-n [number of treepieces]\n");
            CkPrintf("\t-p [maximum number of particles per treepiece]\n");
            CkPrintf("\t-l [maximum number of particles per leaf]\n");
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
        if (n_treepieces <= 0) {
          CkAbort("Number of treepieces must be larger than 0 with SFC decomposition!");
        }
        CkPrintf("Number of treepieces: %d\n", n_treepieces);
      }
      else if (decomp_type == OCT_DECOMP) {
        CkPrintf("Maximum number of particles per treepiece: %d\n", max_particles_per_tp);
      }
      CkPrintf("Maximum number of particles per leaf: %d\n\n", max_particles_per_leaf);

      // create Readers
      n_readers = CkNumNodes();
      readers = CProxy_Reader::ckNew();

      // create Decomposer
      decomposer = CProxy_Decomposer::ckNew(n_treepieces);

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
