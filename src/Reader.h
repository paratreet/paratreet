#ifndef PARATREET_READER_H_
#define PARATREET_READER_H_

#include "paratreet.decl.h"
#include "common.h"
#include "templates.h"
#include "Splitter.h"
#include "SortingInterfaces.h"
#include "Utility.h"
#include "TreeSpec.h"
#include "Modularization.h"

#include <iostream>
#include <cstring>
#include <algorithm>

extern CProxy_TreeSpec treespec;

template <typename Data>
class ReaderProxy : public IReader {
  CProxy_Reader<Data> proxy;
public:
  ReaderProxy(CProxy_Reader<Data> p) : proxy(p) {}
  void doSplit(const std::vector<GenericSplitter>&, bool is_subtree, const CkCallback&) override;
  void countAssignments(const std::vector<GenericSplitter>&, bool is_subtree, const CkCallback& cb, bool weight_by_partition) override;
  void localSortByKey(const CkCallback& cb) override;
};

template <typename Data>
void ReaderProxy<Data>::doSplit(const std::vector<GenericSplitter>& splitters, bool is_subtree, const CkCallback& cb) {
  proxy.doSplit(splitters, is_subtree, cb);
}

template <typename Data>
void ReaderProxy<Data>::countAssignments(const std::vector<GenericSplitter>& splitters, bool is_subtree, const CkCallback& cb, bool weight_by_partition) {
  proxy.countAssignments(splitters, is_subtree, cb, weight_by_partition);
}

template <typename Data>
void ReaderProxy<Data>::localSortByKey(const CkCallback& cb) {
  proxy.localSortByKey(cb);
}

template <typename Data>
class ParticleViewer : public IParticleViewer {
  typename Data::Particle* const particles;
  const size_t n_particles;

public:
  ParticleViewer(std::vector<typename Data::Particle>& p) : particles(p.data()), n_particles(p.size()) {}
  ParticleViewer(typename Data::Particle* p, size_t np) : particles(p), n_particles(np) {}
  Vector3D<Real> position(size_t index) const override;
  Key key(size_t index) const override;
  int partitionIndex(size_t index) const override;
  size_t size() const override;

  void sortByPosition(const PositionComparatorFn& fn) override;
  void sortByKey(const KeyComparatorFn& fn) override;

  void nthElementByPosition(size_t n, const PositionComparatorFn& fn) override;
  void nthElementByKey(size_t n, const KeyComparatorFn& fn) override;
};

template <typename Data>
Vector3D<Real> ParticleViewer<Data>::position(size_t index) const {
  return particles[index].position;
}

template <typename Data>
Key ParticleViewer<Data>::key(size_t index) const {
  return particles[index].key;
}

template <typename Data>
int ParticleViewer<Data>::partitionIndex(size_t index) const {
  return particles[index].partition_idx;
}

template <typename Data>
size_t ParticleViewer<Data>::size() const {
  return n_particles;
}

template <typename Data>
void ParticleViewer<Data>::sortByPosition(const PositionComparatorFn& fn) {
  std::sort(particles, particles + n_particles, [fn] (
    const typename Data::Particle& a, const typename Data::Particle& b) {
    return fn(a.position, b.position);
  });
}

template <typename Data>
void ParticleViewer<Data>::sortByKey(const KeyComparatorFn& fn) {
  std::sort(particles, particles + n_particles, [fn] (
    const typename Data::Particle& a, const typename Data::Particle& b) {
    return fn(a.key, b.key);
  });
}

template <typename Data>
void ParticleViewer<Data>::nthElementByPosition(size_t n, const PositionComparatorFn& fn) {
  std::nth_element(particles, particles + n, particles + n_particles, [fn] (
    const typename Data::Particle& a, const typename Data::Particle& b) {
    return fn(a.position, b.position);
  });
}

template <typename Data>
void ParticleViewer<Data>::nthElementByKey(size_t n, const KeyComparatorFn& fn) {
  std::nth_element(particles, particles + n, particles + n_particles, [fn] (
    const typename Data::Particle& a, const typename Data::Particle& b) {
    return fn(a.key, b.key);
  });
}

template <typename Data>
class Reader : public CBase_Reader<Data> {
  std::vector<typename Data::Particle> saved_particles;

public:
  // Loading particles and assigning keys
  void load(const CkCallback&);
  void computeUniverseBoundingBox(const CkCallback&);
  void adjustParticlesForUniverse(typename Data::BoundingBox, const CkCallback&);

  void countAssignments(const std::vector<GenericSplitter>&, bool is_subtree, const CkCallback&, bool weight_by_partition);
  void doSplit(const std::vector<GenericSplitter>&, bool is_subtree, const CkCallback&);

  void receive(const std::vector<typename Data::Particle>& particles);
  void localSortByKey(const CkCallback&);
  void localSortByOrder(const CkCallback&);
  // Sending particles to home Partitions and Subtrees
  void flush(int, CProxy_Subtree<Data>);
  void flushToSubtreesHelper(std::vector<typename Data::Particle>&, CProxy_Subtree<Data>);
  void assignPartitions(int, CProxy_Partition<Data>);
  void write(int prefix_count, std::string output_file, typename Data::BoundingBox box, int iter, int indicator, CkCallback);
  int numReaders();
};

template <typename Data>
void Reader<Data>::load(const CkCallback& cb) {
  Data::loadParticlesFromFile(this->thisIndex, numReaders(), paratreet::getConfiguration(), saved_particles);
  computeUniverseBoundingBox(cb);
}

template <typename Data>
void Reader<Data>::computeUniverseBoundingBox(const CkCallback& cb) {
  typename Data::BoundingBox box;
  for (auto && p : saved_particles) {
    Data::addParticleToBox(p, box);
  }
#if DEBUG
  std::cout << "[Reader " << this->thisIndex << "] Built bounding box: " << box << std::endl;
#endif
  this->contribute(sizeof(typename Data::BoundingBox), &box, Data::BoundingBox::reducer(), cb);
}

template <typename Data>
void Reader<Data>::adjustParticlesForUniverse(typename Data::BoundingBox universe, const CkCallback& cb) {
  // Generate particle keys
  for (auto && p : saved_particles) {
    Data::adjustParticleForUniverse(p, universe);
  }
  // Back to callee
  this->contribute(cb);
}

template <typename Data>
void Reader<Data>::flush(int n_subtrees, CProxy_Subtree<Data> subtrees) {
  flushToSubtreesHelper(saved_particles, subtrees);
  saved_particles.clear();
}

template <typename Data>
void Reader<Data>::flushToSubtreesHelper(std::vector<typename Data::Particle>& particles, CProxy_Subtree<Data> subtrees) {
  std::vector<int> destinations (particles.size());
  ParticleViewer<Data> viewer {particles};
  bool sorted = treespec.ckLocalBranch()->getSubtreeDecomposition()->flush(&viewer, destinations.data());
  std::set<int> saved_destinations;
  for (size_t di = 0u; di < destinations.size(); di++) {
    auto dest = destinations[di];
    auto inserted = saved_destinations.insert(dest).second;
    if (!inserted) {
      continue;
    }
    std::vector<typename Data::Particle> particle_msg;
    for (size_t pi = di; pi < destinations.size(); pi++) {
      if (destinations[pi] == dest) {
        particle_msg.push_back(particles[pi]);
      }
      else if (sorted) {
	break;
      }
    }
    subtrees[dest].receive(particle_msg);
  }
}

template <typename Data>
void Reader<Data>::countAssignments(const std::vector<GenericSplitter>& states, bool is_subtree, const CkCallback& cb, bool weight_by_partition) {
  auto decomp = is_subtree ? treespec.ckLocalBranch()->getSubtreeDecomposition() : treespec.ckLocalBranch()->getPartitionDecomposition();
  ParticleViewer<Data> viewer {saved_particles};
  auto msg = decomp->countAssignments(states, &viewer, is_subtree, weight_by_partition);
  if (msg) {
    msg->setCallback(cb);
    this->contribute(msg);
  }
}

template <typename Data>
void Reader<Data>::assignPartitions(int n_partitions, CProxy_Partition<Data> partitions)
{
  std::vector<int> destinations (saved_particles.size());
  ParticleViewer<Data> viewer {saved_particles};
  treespec.ckLocalBranch()->getSubtreeDecomposition()->flush(&viewer, destinations.data());
  for (size_t pi = 0u; pi < saved_particles.size(); pi++) {
    saved_particles[pi].partition_idx = destinations[pi];
  }
}

template <typename Data>
void Reader<Data>::doSplit(const std::vector<GenericSplitter>& splits, bool is_subtree, const CkCallback& cb) {
  auto decomp = is_subtree ? treespec.ckLocalBranch()->getSubtreeDecomposition() : treespec.ckLocalBranch()->getPartitionDecomposition();
  auto msg = decomp->doSplit(splits);
  if (msg) {
    msg->setCallback(cb);
    this->contribute(msg);
  }
}

template <typename Data>
void Reader<Data>::receive(const std::vector<typename Data::Particle>& particles) {
  // Copy particles to local vector
  saved_particles.insert(saved_particles.end(), particles.begin(), particles.end());
}


template <typename Data>
void Reader<Data>::localSortByKey(const CkCallback& cb) {
  std::sort(saved_particles.begin(), saved_particles.end());
  this->contribute(cb);
}

template <typename Data>
void Reader<Data>::localSortByOrder(const CkCallback& cb) {
  std::sort(saved_particles.begin(), saved_particles.end(),
            [](const typename Data::Particle& left, const typename Data::Particle& right) {
              return left.order < right.order;
            });
  this->contribute(cb);
}

template <typename Data>
void Reader<Data>::write(int prefix_count, std::string output_file, typename Data::BoundingBox box, int iter, int indicator, CkCallback cb)
{
  Data::outputToFile(this->thisIndex, prefix_count, box, iter, output_file, saved_particles, indicator);
  int new_prefix_count = prefix_count + saved_particles.size();
  if (this->thisIndex != CkNumPes() - 1) {
    this->thisProxy[this->thisIndex + 1].write(new_prefix_count, output_file, box, iter, indicator, cb);
  }
  else {
    cb.send();
  }
  saved_particles.clear();
}

template <typename Data>
int Reader<Data>::numReaders()
{
  return this->isNodeGroup() ? CkNumNodes() : CkNumPes();
}


#endif // PARATREET_READER_H_
