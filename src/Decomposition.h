#ifndef PARATREET_DECOMPOSITION_H_
#define PARATREET_DECOMPOSITION_H_

#include <functional>
#include <vector>

#include "Splitter.h"
#include "SortingInterfaces.h"
#include "Configuration.h"

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

  Decomposition() : PUP::able() {}
  Decomposition(CkMigrateMessage *m) : PUP::able(m) {}
  virtual ~Decomposition() = default;
  virtual void pup(PUP::er& p) override;

  virtual bool flush(IParticleViewer* particles, int* destinations) = 0; // contiguous

  virtual int getNumParticles(int tp_index) = 0;

  virtual int getPartitionHome(int tp_index) = 0;

  virtual CkReductionMsg* countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) = 0;

  virtual int findSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) = 0;

  virtual Key getTpKey(int idx) = 0;

  virtual CkReductionMsg* doSplit(const std::vector<GenericSplitter>& splits);

  virtual void setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate);
};

struct SfcDecomposition : public Decomposition {
  PUPable_decl(SfcDecomposition);

  SfcDecomposition() : Decomposition() {}
  SfcDecomposition(CkMigrateMessage *m) : Decomposition(m) { }
  virtual ~SfcDecomposition() = default;

  virtual Key getTpKey(int idx) override;
  virtual bool flush(IParticleViewer* particles, int* destinations) override;
  virtual int getNumParticles(int tp_index) override;
  virtual int getPartitionHome(int tp_index) override;
  virtual CkReductionMsg* countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) override;
  virtual int findSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) override;
  virtual void alignSplitters(SfcDecomposition *);
  std::vector<Splitter> getSplitters();
  virtual void pup(PUP::er& p) override;

private:
  int parallelFindSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree);
  //int serialFindSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters);

protected:
  std::vector<Splitter> splitters;
  std::vector<int> partition_idxs;
  int saved_n_total_particles = 0;
};

struct OctDecomposition : public SfcDecomposition {
  PUPable_decl(OctDecomposition);

  OctDecomposition() {}
  OctDecomposition(CkMigrateMessage *m) : SfcDecomposition(m) { }
  virtual ~OctDecomposition() = default;
  virtual int getBranchFactor() const {return 8;}

  virtual bool flush(IParticleViewer* particles, int* destinations) override;
  virtual CkReductionMsg* countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) override;
  virtual int findSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) override;
  virtual void setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate) override;
};

struct BinaryOctDecomposition : public OctDecomposition {
  PUPable_decl(BinaryOctDecomposition);

  BinaryOctDecomposition() {}
  BinaryOctDecomposition(CkMigrateMessage *m) : OctDecomposition(m) { }
  virtual ~BinaryOctDecomposition() = default;
  virtual int getBranchFactor() const override {return 2;}
};

struct BinaryDecomposition : public Decomposition {

  BinaryDecomposition() {}
  BinaryDecomposition(CkMigrateMessage *m) : Decomposition(m) { }
  virtual ~BinaryDecomposition() = default;

  virtual void initBinarySplit(const IParticleViewer* particles);

  Key getTpKey(int idx) override;
  bool flush(IParticleViewer* particles, int* destinations) override;
  int getNumParticles(int tp_index) override;
  int getPartitionHome(int tp_index) override;
  int findSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) override;
  CkReductionMsg* doSplit(const std::vector<GenericSplitter>& splits) override;

  virtual void pup(PUP::er& p) override;

  using Bin = std::vector<std::pair<int, Vector3D<Real>>>;
  using BinarySplit = std::pair<int, Real>;
  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) = 0;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) = 0;
  virtual std::vector<GenericSplitter> sortAndGetSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, bool is_subtree) = 0;

private:
  int parallelFindSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree);
  //int serialFindSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters);

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
  KdDecomposition() {}
  KdDecomposition(CkMigrateMessage *m) : BinaryDecomposition(m) { }

  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) override;
  virtual CkReductionMsg* countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) override;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) override;
  virtual std::vector<GenericSplitter> sortAndGetSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, bool is_subtree) override;
};

struct LongestDimDecomposition : public BinaryDecomposition {
  PUPable_decl(LongestDimDecomposition);
  LongestDimDecomposition() {}
  LongestDimDecomposition(CkMigrateMessage *m) : BinaryDecomposition(m) { }

  virtual BinarySplit sortAndGetSplitter(int depth, Bin& bin) override;
  virtual CkReductionMsg* countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) override;
  virtual void assign(Bin& parent, Bin& left, Bin& right, std::pair<int, Real> split) override;
  virtual std::vector<GenericSplitter> sortAndGetSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, bool is_subtree) override;
  void setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate) override;
};

#endif
