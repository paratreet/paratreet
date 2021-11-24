#ifndef EXAMPLE_MAIN_H
#define EXAMPLE_MAIN_H

#include "Main.decl.h"
#include "Paratreet.h"

struct GravityConfiguration: public paratreet::Configuration {
     double dTheta = 0.7;
     
     GravityConfiguration(void) {
          this->register_field("dTheta", false, dTheta);
     }

     GravityConfiguration(CkMigrateMessage* m) : paratreet::Configuration(m) {}

     virtual void pup(PUP::er &p) override {
          paratreet::Configuration::pup(p);
          p | dTheta;
     }

     PUPable_decl_inside(GravityConfiguration);
};

#include "CentroidData.h"

class ExMain: public paratreet::Main<CentroidData, GravityConfiguration> {
  virtual Real getTimestep(BoundingBox&, Real) override;
  virtual void preTraversalFn(ProxyPack<CentroidData>&) override;
  virtual void traversalFn(BoundingBox&, ProxyPack<CentroidData>&, int) override;
  virtual void postIterationFn(BoundingBox&, ProxyPack<CentroidData>&, int) override;
  virtual void main(CkArgMsg*) override;
  virtual void run(void) override;
};

#endif
