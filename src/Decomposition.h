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
  virtual ~Decomposition() = default;
  virtual int flush(int n_total_particles, int n_treepieces,
      const SendParticlesFn &fn, std::vector<Particle> &particles) = 0;

  virtual void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) = 0;

  virtual int getNumExpectedParticles(int n_total_particles, int n_treepieces, int tp_index) = 0;

  virtual int findSplitters(BoundingBox &universe, CProxy_Reader &readers) = 0;

  virtual void pup(PUP::er& p) = 0;

  virtual int getTpKey(int idx) = 0;

  std::vector<Key> getAllTpKeys(int n_treepieces) {
    std::vector<Key> tp_keys (n_treepieces);
    for (int i = 0; i < n_treepieces; i++) {
      tp_keys[i] = getTpKey(i);
    }
    return tp_keys;
  }
};

struct SfcDecomposition : public Decomposition {
  int getTpKey(int idx) override;
  int flush(int n_total_particles, int n_treepieces,
      const SendParticlesFn &fn, std::vector<Particle> &particles) override;
  void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) override;
  int getNumExpectedParticles(int n_total_particles, int n_treepieces, int tp_index) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers) override;
  virtual void pup(PUP::er& p) override;
protected:
  std::vector<Splitter> splitters;
};

struct OctDecomposition : public SfcDecomposition {
  int flush(int n_total_particles, int n_treepieces,
      const SendParticlesFn &fn, std::vector<Particle> &particles) override;
  void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) override;
  int getNumExpectedParticles(int n_total_particles, int n_treepieces, int tp_index) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers) override;
};

#endif
