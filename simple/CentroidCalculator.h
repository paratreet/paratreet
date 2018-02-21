#ifndef SIMPLE_CENTROIDCALCULATOR_H_
#define SIMPLE_CENTROIDCALCULATOR_H_

#include "simple.decl.h"
#include "CentroidVisitor.h"
#include "CentroidData.h"

class CentroidCalculator : public CBase_CentroidCalculator {
private:
  CentroidData cd;
  CentroidVisitor v;
  int wait_count;
public:
  CentroidCalculator();
  void receiveCentroid (CentroidData, bool);
};

#endif
