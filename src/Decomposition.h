#ifndef PARATREET_DECOMPOSITION_H_
#define PARATREET_DECOMPOSITION_H_

#include <functional>
#include <vector>

class CProxy_Reader;
class BoundingBox;
class Splitter;
class Particle;
namespace paratreet {
  class Configuration;
}

using SendProxyFn = std::function<void(Key,int)>;
using SendParticlesFn = std::function<void(int,int,Particle*)>;

struct Decomposition: public PUP::able {
  PUPable_abstract(Decomposition);

  Decomposition() { }
  Decomposition(CkMigrateMessage *m) : PUP::able(m) { }
  virtual ~Decomposition() = default;

  virtual int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) = 0;

  virtual void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) = 0;

  virtual int getNumExpectedParticles(int n_total_particles, int n_partitions, int tp_index) = 0;

  virtual int findSplitters(BoundingBox &universe, CProxy_Reader &readers, const paratreet::Configuration& config, int log_branch_factor) = 0;

  virtual Key getTpKey(int idx) = 0;

  std::vector<Key> getAllTpKeys(int n_partitions) {
    std::vector<Key> tp_keys (n_partitions);
    for (int i = 0; i < n_partitions; i++) {
      tp_keys[i] = getTpKey(i);
    }
    return tp_keys;
  }
};

struct SfcDecomposition : public Decomposition {
  PUPable_decl(SfcDecomposition);

  SfcDecomposition() { }
  SfcDecomposition(CkMigrateMessage *m) : Decomposition(m) { }
  virtual ~SfcDecomposition() = default;

  Key getTpKey(int idx) override;
  int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) override;
  void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) override;
  int getNumExpectedParticles(int n_total_particles, int n_partitions, int tp_index) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers, const paratreet::Configuration& config, int log_branch_factor) override;

  void alignSplitters(SfcDecomposition *);
  std::vector<Splitter> getSplitters();
  virtual void pup(PUP::er& p) override;

protected:
  std::vector<Splitter> splitters;
};

struct OctDecomposition : public SfcDecomposition {
  PUPable_decl(OctDecomposition);

  OctDecomposition() { }
  OctDecomposition(CkMigrateMessage *m) : SfcDecomposition(m) { }
  virtual ~OctDecomposition() = default;

  int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) override;
  void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) override;
  int getNumExpectedParticles(int n_total_particles, int n_partitions, int tp_index) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers, const paratreet::Configuration& config, int log_branch_factor) override;
};

struct KdDecomposition : public Decomposition {
  PUPable_decl(KdDecomposition);

  KdDecomposition() = default;
  KdDecomposition(CkMigrateMessage *m) : Decomposition(m) { }
  virtual ~KdDecomposition() = default;

  Key getTpKey(int idx) override;
  int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) override;
  void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) override;
  int getNumExpectedParticles(int n_total_particles, int n_partitions, int tp_index) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers, const paratreet::Configuration& config, int log_branch_factor) override;

  virtual void pup(PUP::er& p) override;
};


#endif
