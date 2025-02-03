#ifndef ASTRO_MAIN_H_
#define ASTRO_MAIN_H_

#include "CentroidData.h"
#include "Main.decl.h"
#include "Paratreet.h"
#include "AstroFields.h"

struct AstroConfiguration : public paratreet::Configuration {
  AstroFields astro;
  AstroConfiguration();
  AstroConfiguration(CkMigrateMessage *m);
  PUPable_decl_inside(AstroConfiguration);
  virtual void pup(PUP::er &p) override;
};

class ExMain: public paratreet::Main<CentroidData, AstroConfiguration> {
  virtual Real getTimestep(const BoundingBox&, Real) override;
  virtual void preTraversalFn(ProxyPack<CentroidData>&) override;
  virtual void traversalFn(const BoundingBox&, ProxyPack<CentroidData>&, int) override;
  virtual void postIterationFn(const BoundingBox&, ProxyPack<CentroidData>&, int) override;
  virtual void setDefaults(void) override;
  virtual void main(CkArgMsg*) override;
  virtual void run(void) override;
};

#endif // ASTRO_MAIN_H_
