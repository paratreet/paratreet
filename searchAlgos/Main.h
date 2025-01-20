#ifndef EXAMPLE_MAIN_H
#define EXAMPLE_MAIN_H

#include "Main.decl.h"
#include "Paratreet.h"
#include "SearchData.h"

class ExMain: public paratreet::Main<SearchData> {
  virtual Real getTimestep(const BoundingBox&, Real) override;
  virtual void preTraversalFn(ProxyPack<SearchData>&) override;
  virtual void traversalFn(const BoundingBox&, ProxyPack<SearchData>&, int) override;
  virtual void postIterationFn(const BoundingBox&, ProxyPack<SearchData>&, int) override;
  virtual void setDefaults(void) override;
  virtual void main(CkArgMsg*) override;
  virtual void run(void) override;
};

#endif
