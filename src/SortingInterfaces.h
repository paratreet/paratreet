#ifndef PARATREET_SORTINGINTERFACES_H_
#define PARATREET_SORTINGINTERFACES_H_

#include "common.h"
#include "Splitter.h"

struct IReader {
  virtual void doSplit(const std::vector<GenericSplitter>&, bool is_subtree, const CkCallback&) = 0;
  virtual void countAssignments(const std::vector<GenericSplitter>&, bool is_subtree, const CkCallback& cb, bool weight_by_partition) = 0;
  virtual void localSortByKey(const CkCallback& cb) = 0;
};

struct IParticleViewer {
  using PositionComparatorFn = std::function<bool(Vector3D<Real>, Vector3D<Real>)>;
  using KeyComparatorFn = std::function<bool(Key, Key)>;
  using OrderComparatorFn = std::function<bool(int, int)>;

  virtual Vector3D<Real> position(size_t index) const = 0;
  virtual Key key(size_t index) const = 0;
  virtual int partitionIndex(size_t index) const = 0;
  virtual size_t size() const = 0;

  virtual void sortByPosition(const PositionComparatorFn& fn) = 0;
  virtual void sortByKey(const KeyComparatorFn& fn) = 0;

  virtual void nthElementByPosition(size_t n, const PositionComparatorFn& fn) = 0;
  virtual void nthElementByKey(size_t n, const KeyComparatorFn& fn) = 0;
};

#endif // PARATREET_SORTINGINTERFACES_H_
