#ifndef SIMPLE_READER_H_
#define SIMPLE_READER_H_

#include "simple.decl.h"
#include "common.h"
#include "ParticleMsg.h"
#include "BoundingBox.h"
#include "Splitter.h"

#include "Utility.h"

extern CProxy_Main mainProxy;
extern int n_readers;
extern int decomp_type;

class Reader : public CBase_Reader {
  BoundingBox box;
  std::vector<Particle> particles;
  std::vector<ParticleMsg*> particle_messages;
  int particle_index;

  public:
    std::vector<Splitter> splitters;
    std::vector<Key> SFCsplitters;
    Reader();

    // loading particles and assigning keys
    void load(std::string, const CkCallback&);
    void computeUniverseBoundingBox(const CkCallback& cb);
    void assignKeys(BoundingBox&, const CkCallback&);

    // OCT decomposition
    void countOct(std::vector<Key>&, const CkCallback&);
    void setSplitters(const std::vector<Splitter>&, const CkCallback&);

    // SFC decomposition
    //void countSfc(const std::vector<Key>&, const CkCallback&);
    void pickSamples(const int, const CkCallback&);
    void prepMessages(const std::vector<Key>&, const CkCallback&);
    void redistribute();
    void receive(ParticleMsg*);
    void localSort(const CkCallback&);
    void checkSort(const Key, const CkCallback&);
    template <typename Data>
    void request(CProxy_TreePiece<Data>, int, int);

    // sending particles to home TreePieces
    template <typename Data>
    void flush(int, int, CProxy_TreePiece<Data>);
};

template <typename Data>
void Reader::request(CProxy_TreePiece<Data> tp_proxy, int index, int num_to_give) {
  int n_particles = box.n_particles;
  ParticleMsg* msg = new (num_to_give) ParticleMsg(&particles[0], num_to_give);
  for (int i = 0; i < n_particles - num_to_give; i++) {
    particles[i] = particles[num_to_give + i];
  } // maybe a function can just do this for me?
  particles.resize(n_particles - num_to_give);
  n_particles -= num_to_give;
  if (n_particles) SFCsplitters.push_back(particles[0].key);
  tp_proxy[index].receive(msg);
}

template <typename Data>
void Reader::flush(int n_total_particles, int n_treepieces, CProxy_TreePiece<Data> treepieces) {
  int flush_count = 0;

  if (decomp_type == OCT_DECOMP) {
    // OCT decomposition
    int start = 0;
    int finish = particles.size();

    // find particles that belong to each splitter range and flush them
    for (int i = 0; i < splitters.size(); i++) {
      int begin = Utility::binarySearchGE(splitters[i].from, &particles[0], start, finish);
      int end = Utility::binarySearchGE(splitters[i].to, &particles[0], begin, finish);

      int n_particles = end - begin;

      if (n_particles > 0) {
        ParticleMsg* msg = new (n_particles) ParticleMsg(&particles[begin], n_particles);
        treepieces[i].receive(msg);
        flush_count += n_particles;
      }

      start = end;
    }

    // free splitter memory
    splitters.resize(0);
  }
  else if (decomp_type == SFC_DECOMP) {
    // TODO SFC decomposition
    // probably need to use prefix sum
    int n_particles_left = particles.size();
    for (int i = 0; i < n_treepieces; i++) {
      int n_need = n_total_particles / n_treepieces;
      if (i < (n_total_particles % n_treepieces))
        n_need++;

      if (n_particles_left > n_need) {
        ParticleMsg* msg = new (n_need) ParticleMsg(&particles[flush_count], n_need);
        treepieces[i].receive(msg);
        flush_count += n_need;
        n_particles_left -= n_need;
      }
      else {
        if (n_particles_left > 0) {
          ParticleMsg* msg = new (n_particles_left) ParticleMsg(&particles[flush_count], n_particles_left);
          treepieces[i].receive(msg);
          flush_count += n_particles_left;
          n_particles_left = 0;
        }
      }

      if (n_particles_left == 0)
        break;
    }
  }

  if (flush_count != particles.size()) {
    CkPrintf("[Reader %d] ERROR! Flushed %d out of %d particles\n", thisIndex, flush_count, particles.size());
    CkAbort("Flush failure");
  }

  // clean up
  particles.resize(0);
  particle_index = 0;
}

#endif // SIMPLE_READER_H_
