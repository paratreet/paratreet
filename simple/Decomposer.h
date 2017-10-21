#ifndef SIMPLE_DECOMPOSER_H_
#define SIMPLE_DECOMPOSER_H_

#include "simple.decl.h"
#include "common.h"
#include "Splitter.h"

class Decomposer : public CBase_Decomposer {
  double start_time;
  CkReductionMsg* result;
  BoundingBox universe;
  CkVec<Splitter> splitters;
  int n_treepieces; // OCT decomposition

  /*
  std::vector<Key> splitters;
  std::vector<int> bin_counts;
  int* splitter_goals;
  int num_goals_pending;
  int tol_diff;
  int num_iterations = 0;
  bool sorted; // flag to tell if we are done

  // members added for adjustSplitters
  CkBitVector bins_to_split;

  std::vector<Key> final_splitters;
  std::vector<int> accumulated_bin_counts;
  */

  public:
    Decomposer();
    void run();
    void findSplitters();
    //void adjustSplitters();
};

#endif // SIMPLE_DECOMPOSER_H_
