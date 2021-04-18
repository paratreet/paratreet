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

struct Decomposition;
class DecompArrayMap : public CkArrayMap {
public:
  DecompArrayMap(Decomposition*, int, int);
  int procNum(int, const CkArrayIndex &idx);
private:
  std::vector<size_t> pe_intervals;
};

struct Decomposition: public PUP::able {
  PUPable_abstract(Decomposition);

  Decomposition() { }
  Decomposition(CkMigrateMessage *m) : PUP::able(m) { }
  virtual ~Decomposition() = default;

  virtual int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) = 0;

  virtual void assignKeys(BoundingBox &universe, std::vector<Particle> &particles);

  virtual int getNumParticles(int tp_index) = 0;

  virtual int findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) = 0;

  virtual Key getTpKey(int idx) = 0;

  virtual void setArrayOpts(CkArrayOptions& opts) {
  }

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
  int getNumParticles(int tp_index) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) override;
  void alignSplitters(SfcDecomposition *);
  std::vector<Splitter> getSplitters();
  virtual void pup(PUP::er& p) override;

private:
  int parallelFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters);
  int serialFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters);

protected:
  std::vector<Splitter> splitters;
  int saved_n_total_particles = 0;
};

struct OctDecomposition : public SfcDecomposition {
  PUPable_decl(OctDecomposition);

  OctDecomposition() { }
  OctDecomposition(CkMigrateMessage *m) : SfcDecomposition(m) { }
  virtual ~OctDecomposition() = default;
  virtual int getBranchFactor() {return 8;}

  int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) override;
  void setArrayOpts(CkArrayOptions& opts) override;
};

struct BinaryOctDecomposition : public OctDecomposition {
  PUPable_decl(BinaryOctDecomposition);

  BinaryOctDecomposition() { }
  BinaryOctDecomposition(CkMigrateMessage *m) : OctDecomposition(m) { }
  virtual ~BinaryOctDecomposition() = default;
  virtual int getBranchFactor() override {return 2;}
};


struct BinaryDecomposition : public Decomposition {
  BinaryDecomposition() = default;
  BinaryDecomposition(CkMigrateMessage *m) : Decomposition(m) { }
  virtual ~BinaryDecomposition() = default;

  Key getTpKey(int idx) override;
  int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) override;
  int getNumParticles(int tp_index) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) override;

  virtual void pup(PUP::er& p) override;

  using Bin = std::vector<Vector3D<Real>>;
  using BinarySplit = std::pair<int, Real>;
  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) = 0;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) = 0;
  virtual std::vector<BinarySplit> sortAndGetSplitters(BoundingBox &universe, CProxy_Reader &readers) = 0;

private:
  int serialFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters);
  int parallelFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters);

protected:
  std::vector<BinarySplit> splitters; //dim, splitter value
  size_t depth = 0;
  int saved_n_total_particles = 0;
  std::vector<int> bins_sizes;
};

struct KdDecomposition : public BinaryDecomposition {
  PUPable_decl(KdDecomposition);
  KdDecomposition() = default;
  KdDecomposition(CkMigrateMessage *m) : BinaryDecomposition(m) { }

  virtual void pup(PUP::er& p) override;
  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) override;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) override;
  virtual std::vector<BinarySplit> sortAndGetSplitters(BoundingBox &universe, CProxy_Reader &readers) override;
};

struct LongestDimDecomposition : public BinaryDecomposition {
  PUPable_decl(LongestDimDecomposition);
  LongestDimDecomposition() = default;
  LongestDimDecomposition(CkMigrateMessage *m) : BinaryDecomposition(m) { }

  virtual void pup(PUP::er& p) override;
  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) override;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) override;
  virtual std::vector<BinarySplit> sortAndGetSplitters(BoundingBox &universe, CProxy_Reader &readers) override;
  void setArrayOpts(CkArrayOptions& opts) override;
};


#endif
