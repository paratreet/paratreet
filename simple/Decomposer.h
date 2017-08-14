#ifndef SIMPLE_DECOMPOSER_H_
#define SIMPLE_DECOMPOSER_H_

#include "simple.decl.h"
#include "common.h"
#include "Splitter.h"

class Decomposer : public CBase_Decomposer {
  CkVec<Splitter> splitters;
  int n_splitters;
  double start_time;

  public:
    Decomposer();
    void run(const CkCallback&);
    void createSplitters();
    void flush(const CkCallback&);
};

#endif // SIMPLE_DECOMPOSER_H_
