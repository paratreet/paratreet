#ifndef EXAMPLE_MAIN_H
#define EXAMPLE_MAIN_H

#include "Main.decl.h"
#include "Paratreet.h"
#include "SearchData.h"

class ExMain: public paratreet::Main<SearchData> {
  virtual Real getTimestep(BoundingBox&, Real) override;
  virtual void preTraversalFn(ProxyPack<SearchData>&) override;
  virtual void traversalFn(BoundingBox&, ProxyPack<SearchData>&, int) override;
  virtual void postIterationFn(BoundingBox&, ProxyPack<SearchData>&, int) override;
  virtual void main(CkArgMsg*) override;
  virtual void run(void) override;
};

#endif
