#ifndef SIMPLE_DECOMPOSER_H_
#define SIMPLE_DECOMPOSER_H_

#include "simple.decl.h"
#include "common.h"
#include "Splitter.h"
#include "TreeElements.h"

class Decomposer : public CBase_Decomposer {
  BoundingBox universe;

  CProxy_TreePiece treepieces; // cannot be a global variable
  CProxy_TreeElements tree_array;
  int n_treepieces;

  CkVec<Splitter> oct_splitters;
  CkVec<Splitter> sfc_splitters;

  public:
    Decomposer(int n_treepieces);
    void run();
    void findOctSplitters();
    void findSfcSplitters();
    bool adjustSplitters();
};

#endif // SIMPLE_DECOMPOSER_H_
