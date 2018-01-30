#ifndef SIMPLE_DECOMPOSER_H_
#define SIMPLE_DECOMPOSER_H_

#include "simple.decl.h"
#include "common.h"
#include "Splitter.h"
#include "TreeElements.h"

class Decomposer : public CBase_Decomposer {
  BoundingBox universe;
  Key smallest_particle_key;
  Key largest_particle_key;

  //CProxy_TreePiece treepieces; // cannot be a global variable
  CProxy_TreeElements tree_array;
  int n_treepieces;

  std::vector<Splitter> splitters;

  public:
    Decomposer(int n_treepieces);
    void run();
    void findOctSplitters();
    void globalSampleSort();
    /*
    void findSfcSplitters();
    bool modifySfcSplitters(std::vector<Key>&, std::vector<Key>&, std::vector<int>&,
        int&, std::vector<int>&, std::vector<int>&, const int);
    */
};

#endif // SIMPLE_DECOMPOSER_H_
