#ifndef PARATREET_DECOMPOSITION_H_
#define PARATREET_DECOMPOSITION_H_

#include <functional>
#include <vector>

class CProxy_Reader;
class BoundingBox;
class Splitter;
class Particle;

using SendProxyFn = std::function<void(int,int)>;
using SendParticlesFn = std::function<void(int,int,Particle*)>;

struct Decomposition {
  virtual int flush(int n_total_particles, int n_treepieces, const SendParticlesFn &fn,
      std::vector<Particle> &particles, std::vector<Splitter> &splitters) = 0;

  virtual void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) = 0;

  virtual int getNumExpectedParticles(int n_total_particles, int n_treepieces,
      int tp_index, const std::vector<Splitter> &splitters) = 0;

  virtual int findSplitters(BoundingBox &universe, CProxy_Reader &readers, std::vector<Splitter> &splitters) = 0;
};

Decomposition* getDecomposition();

#endif
