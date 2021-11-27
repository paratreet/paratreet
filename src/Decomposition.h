#ifndef PARATREET_DECOMPOSITION_H_
#define PARATREET_DECOMPOSITION_H_

#include <functional>
#include <vector>

#include "Splitter.h"
#include "Configuration.h"

class Reader;
class CProxy_Reader;
class BoundingBox;
class Splitter;
class Particle;

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

class CollocateMap : public CkArrayMap {
public:
  CollocateMap(Decomposition* d, const std::vector<int>& partition_locations);
  int procNum(int, const CkArrayIndex &idx);

private:
  Decomposition* const decomp;
  const std::vector<int> partition_locations;
};


struct Decomposition: public PUP::able {
  PUPable_abstract(Decomposition);

  Decomposition(bool is_subtree_) : is_subtree(is_subtree_) { }
  Decomposition(CkMigrateMessage *m) : PUP::able(m) {}
  virtual ~Decomposition() = default;
  virtual void pup(PUP::er& p) override;

  virtual int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) = 0;

  virtual void assignKeys(BoundingBox &universe, std::vector<Particle> &particles);

  virtual int getNumParticles(int tp_index) = 0;

  virtual int getPartitionHome(int tp_index) = 0;

  virtual void countAssignments(const std::vector<GenericSplitter>& states, const std::vector<Particle>& particles, Reader* reader, const CkCallback& cb, bool weight_by_partition) = 0;

  virtual void doSplit(const std::vector<GenericSplitter>& splits, Reader* reader, const CkCallback& cb) {}

  virtual int findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) = 0;

  virtual Key getTpKey(int idx) = 0;

  virtual void setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate);

  std::vector<Key> getAllTpKeys(int n_partitions) {
    std::vector<Key> tp_keys (n_partitions);
    for (int i = 0; i < n_partitions; i++) {
      tp_keys[i] = getTpKey(i);
    }
    return tp_keys;
  }

protected:
  bool isSubtree() const {return is_subtree;}
private:
  bool is_subtree = false; // cant use const cause pup()
};

struct SfcDecomposition : public Decomposition {
  PUPable_decl(SfcDecomposition);

  SfcDecomposition(bool is_subtree) : Decomposition(is_subtree) {}
  SfcDecomposition(CkMigrateMessage *m) : Decomposition(m) { }
  virtual ~SfcDecomposition() = default;

  virtual Key getTpKey(int idx) override;
  virtual int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) override;
  virtual int getNumParticles(int tp_index) override;
  virtual int getPartitionHome(int tp_index) override;
  virtual void countAssignments(const std::vector<GenericSplitter>& states, const std::vector<Particle>& particles, Reader* reader, const CkCallback& cb, bool weight_by_partition) override;
  virtual int findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) override;
  virtual void alignSplitters(SfcDecomposition *);
  std::vector<Splitter> getSplitters();
  virtual void pup(PUP::er& p) override;

private:
  int parallelFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters);
  int serialFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters);

protected:
  std::vector<Splitter> splitters;
  std::vector<int> partition_idxs;
  int saved_n_total_particles = 0;
};

struct OctDecomposition : public SfcDecomposition {
  PUPable_decl(OctDecomposition);

  OctDecomposition(bool is_subtree) : SfcDecomposition(is_subtree) {}
  OctDecomposition(CkMigrateMessage *m) : SfcDecomposition(m) { }
  virtual ~OctDecomposition() = default;
  virtual int getBranchFactor() const {return 8;}

  virtual int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) override;
  virtual void countAssignments(const std::vector<GenericSplitter>& states, const std::vector<Particle>& particles, Reader* reader, const CkCallback& cb, bool weight_by_partition) override;
  virtual int findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) override;
  virtual void setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate) override;
};

struct BinaryOctDecomposition : public OctDecomposition {
  PUPable_decl(BinaryOctDecomposition);

  BinaryOctDecomposition(bool is_subtree) : OctDecomposition(is_subtree) {}
  BinaryOctDecomposition(CkMigrateMessage *m) : OctDecomposition(m) { }
  virtual ~BinaryOctDecomposition() = default;
  virtual int getBranchFactor() const override {return 2;}
};


struct BinaryDecomposition : public Decomposition {

  BinaryDecomposition(bool is_subtree) : Decomposition(is_subtree) {}
  BinaryDecomposition(CkMigrateMessage *m) : Decomposition(m) { }
  virtual ~BinaryDecomposition() = default;

  virtual void initBinarySplit(const std::vector<Particle>& particles);

  Key getTpKey(int idx) override;
  int flush(std::vector<Particle> &particles, const SendParticlesFn &fn) override;
  int getNumParticles(int tp_index) override;
  int getPartitionHome(int tp_index) override;
  int findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) override;
  void doSplit(const std::vector<GenericSplitter>& splits, Reader* reader, const CkCallback& cb) override;

  virtual void pup(PUP::er& p) override;

  using Bin = std::vector<std::pair<int, Vector3D<Real>>>;
  using BinarySplit = std::pair<int, Real>;
  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) = 0;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) = 0;
  virtual std::vector<GenericSplitter> sortAndGetSplitters(BoundingBox &universe, CProxy_Reader &readers) = 0;

private:
  int serialFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters);
  int parallelFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters);

protected:
  std::vector<GenericSplitter> splitters; //dim, splitter value
  size_t depth = 0;
  int saved_n_total_particles = 0;
  std::vector<int> bins_sizes;
  std::vector<int> partition_idxs;
  std::vector<Bin> bins;
};

struct KdDecomposition : public BinaryDecomposition {
  PUPable_decl(KdDecomposition);
  KdDecomposition(bool is_subtree) : BinaryDecomposition(is_subtree) {}
  KdDecomposition(CkMigrateMessage *m) : BinaryDecomposition(m) { }

  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) override;
  virtual void countAssignments(const std::vector<GenericSplitter>& states, const std::vector<Particle>& particles, Reader* reader, const CkCallback& cb, bool weight_by_partition) override;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) override;
  virtual std::vector<GenericSplitter> sortAndGetSplitters(BoundingBox &universe, CProxy_Reader &readers) override;
};

struct LongestDimDecomposition : public BinaryDecomposition {
  PUPable_decl(LongestDimDecomposition);
  LongestDimDecomposition(bool is_subtree) : BinaryDecomposition(is_subtree) {}
  LongestDimDecomposition(CkMigrateMessage *m) : BinaryDecomposition(m) { }

  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) override;
  virtual void countAssignments(const std::vector<GenericSplitter>& states, const std::vector<Particle>& particles, Reader* reader, const CkCallback& cb, bool weight_by_partition) override;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) override;
  virtual std::vector<GenericSplitter> sortAndGetSplitters(BoundingBox &universe, CProxy_Reader &readers) override;
  void setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate) override;
};


#endif
