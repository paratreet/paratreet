#ifndef SIMPLE_DECOMPOSER_H_
#define SIMPLE_DECOMPOSER_H_

#include "simple.decl.h"
#include "common.h"

class Decomposer : public CBase_Decomposer {
  double start_time;
  CkReductionMsg* result;
  BoundingBox universe;
  CkVec<Key> splitters;
  std::vector<int> bin_counts;
  int tol_diff;

  public:
    Decomposer();
    void run();
    void findSplitters();
};

#endif // SIMPLE_DECOMPOSER_H_
