#ifndef PARATREET_READER_H_
#define PARATREET_READER_H_

#include "paratreet.decl.h"
#include "common.h"
#include "ParticleMsg.h"
#include "BoundingBox.h"
#include "Splitter.h"
#include "Utility.h"
#include "TreeSpec.h"
#include "Modularization.h"

extern int n_readers;
extern CProxy_TreeSpec treespec;

class Reader : public CBase_Reader {
  std::vector<Particle> particles;
  std::vector<ParticleMsg*> particle_messages;
  int particle_index;

  static constexpr const Real gasConstant = 1.0;
  static constexpr const Real gammam1 = 5.0/3.0 - 1;
  static constexpr const Real meanMolWeight = 1.0;

  public:
    Real start_time = 0;
    BoundingBox universe;
    // std::vector<Splitter> splitters;
    // std::vector<Key> SFCsplitters;
    Reader();

    // Loading particles and assigning keys
    void load(std::string, const CkCallback&);
    void setSoft(const double dSoft, const CkCallback&);
    void computeUniverseBoundingBox(const CkCallback& cb);
    void assignKeys(BoundingBox, const CkCallback&);

    void countAssignments(const std::vector<GenericSplitter>&, bool is_subtree, const CkCallback& cb, bool weight_by_partition);
    void doSplit(const std::vector<GenericSplitter>&, bool is_subtree, const CkCallback&);

    // SFC decomposition
    void getAllSfcKeys(const CkCallback& cb);
    void getAllPositions(const CkCallback& cb);
    //void countSfc(const std::vector<Key>&, const CkCallback&);
    void pickSamples(const int, const CkCallback&);
    void prepMessages(const std::vector<Key>&, const CkCallback&);
    void redistribute();
    void receive(ParticleMsg*);
    void localSort(const CkCallback&);
    void checkSort(const Key, const CkCallback&);
    template <typename Data>
    void request(CProxy_Subtree<Data>, int, int);

    // Sending particles to home Partitions and Subtrees
    template <typename Data>
    void flush(int, CProxy_Subtree<Data>);
    template <typename Data>
    void assignPartitions(int, CProxy_Partition<Data>);
};

template <typename Data>
void Reader::request(CProxy_Subtree<Data> tp_proxy, int index, int num_to_give) {
  int n_particles = universe.n_particles;
  ParticleMsg* msg = new (num_to_give) ParticleMsg(&particles[0], num_to_give);
  for (int i = 0; i < n_particles - num_to_give; i++) {
    particles[i] = particles[num_to_give + i];
  } // maybe a function can just do this for me?
  particles.resize(n_particles - num_to_give);
  n_particles -= num_to_give;
  // if (n_particles) SFCsplitters.push_back(particles[0].key);
  tp_proxy[index].receive(msg);
}

template <typename Data>
void Reader::flush(int n_subtrees,
                   CProxy_Subtree<Data> subtrees) {
  auto sendParticles = [&](int dest, int n_particles, Particle* particles) {
    ParticleMsg* msg = new (n_particles) ParticleMsg(particles, n_particles);
    subtrees[dest].receive(msg);
  };

  int flush_count = treespec.ckLocalBranch()->getSubtreeDecomposition()->flush(particles, sendParticles);
  if (flush_count != particles.size()) {
    CkPrintf("Reader %d failure: flushed %d out of %zu particles\n", thisIndex,
        flush_count, particles.size());
    CkAbort("Reader failure -- see stdout");
  }

  // Clean up
  particles.clear();
  particle_index = 0;
}

template <typename Data>
void Reader::assignPartitions(int n_partitions, CProxy_Partition<Data> partitions)
{
  auto sendParticles =
    [&](int dest, int n_particles, Particle* particles) {
      for (int i = 0; i < n_particles; ++i)
        particles[i].partition_idx = dest;
    };
  int flush_count = treespec.ckLocalBranch()->getPartitionDecomposition()->flush(particles, sendParticles);
  if (flush_count != particles.size()) {
    CkPrintf("Reader %d failure: flushed %d out of %zu particles\n", thisIndex,
        flush_count, particles.size());
    CkAbort("Reader failure -- see stdout");
  }
}


#endif // PARATREET_READER_H_
