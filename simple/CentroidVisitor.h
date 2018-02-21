#ifndef SIMPLE_CENTROIDVISITOR_H_
#define SIMPLE_CENTROIDVISITOR_H_

//#include "simple.decl.h"
//#include "CentroidCalculator.h"
#include "common.h"

class CentroidVisitor {
public:
  //CentroidVisitor() {CkPrintf("hi\n");}
  void leaf(Vector3D<Real>, Real, Key);
  void node(Vector3D<Real>, Real, Key);
  void pup(PUP::er &p) {}
};

#endif //SIMPLE_CENTROIDVISITOR_H_
