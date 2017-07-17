#include "simple.decl.h"
#include "TipsyFile.h"
#include <string>

/* readonly */ CProxy_Main mainProxy;
/* readonly */ int initial_depth;

class Main : public CBase_Main {
  CProxy_Reader readers;
  double start_time;
  std::string input_file;

  public:
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

      // record start time
      start_time = CkWallTimer();

      // create readers
      readers = CProxy_Reader::ckNew();

      // load Tipsy data
      readers.load(input_file);
    }
};

class Reader : public CBase_Reader {
  public:
    Reader() {}

    void load(std::string input_file) {
      Tipsy::TipsyReader r(input_file);
      if (!r.status()) {
        CkPrintf("[%u] Could not open tipsy file (%s)\n", thisIndex, input_file.c_str());
        CkExit();
      }
    }
};

#include "simple.def.h"
