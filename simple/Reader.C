#include "TipsyFile.h"
#include "Reader.h"

Reader::Reader() {}

void Reader::load(std::string input_file) {
  Tipsy::TipsyReader r(input_file);
  if (!r.status()) {
    CkPrintf("[%u] Could not open tipsy file (%s)\n", thisIndex, input_file.c_str());
    CkExit();
  }
}
