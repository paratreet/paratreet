#ifndef SIMPLE_DECOMPOSER_H_
#define SIMPLE_DECOMPOSER_H_

#include "simple.decl.h"
#include "common.h"

class Decomposer : public CBase_Decomposer {
  double start_time;
  CkReductionMsg* result;
  BoundingBox universe;
  std::vector<Key> splitters;
  std::vector<int> bin_counts;
  int* splitter_goals;
  int num_goals_pending;
  int tol_diff;

  std::vector<Key> final_splitters;
  std::vector<int> accumulated_bin_counts;

  bool sorted; // flag to tell if we are done

  // members added for adjustSplitters
  CkBitVector bins_to_split;

  public:
    Decomposer();
    void run();
    void findSplitters();
    void adjustSplitters();
};

#endif // SIMPLE_DECOMPOSER_H_
