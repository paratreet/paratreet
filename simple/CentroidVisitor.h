#ifndef SIMPLE_CENTROIDVISITOR_H_
#define SIMPLE_CENTROIDVISITOR_H_

#include "simple.decl.h"
#include "CentroidData.h"
#include "common.h"

extern CProxy_Main mainProxy;

class CentroidVisitor {
private:
  DataInterface<CentroidVisitor, CentroidData> d;
public:
  CentroidVisitor() {}
  CentroidVisitor(DataInterface<CentroidVisitor, CentroidData> di) : d(di) {}
  void leaf(Key key, int n_particles, Particle* particles) {
    CentroidData cd;
    for (int i = 0; i < n_particles; i++) {
      Particle p = particles[i];
      cd.moment += particles[i].mass * particles[i].position;
      cd.sum_mass += particles[i].mass;
    }
    d.contribute(key, cd);
  }
  bool node(CentroidData cd, Key key) {
    if (key == 1) {
      //CkPrintf("%f %f %f %f\n", cd.sum_mass, cd.moment.x, cd.moment.y, cd.moment.x);
      mainProxy.doneTraversal();
      return false;
    }
    return d.contribute(key, cd);
  }
  void pup(PUP::er& p) {
    p | d;
  }
};

#endif //SIMPLE_CENTROIDVISITOR_H_
