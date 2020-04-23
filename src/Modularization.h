
#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "BufferedVec.h"
#include "Reader.h"
#include "Particle.h"
#include "Splitter.h"
#include "Utility.h"

#include <functional>

extern int max_particles_per_tp; // for OCT decomposition

class SfcDecomposition {
public:
  
  template <typename Data>
  static int flush(int n_total_particles, int n_treepieces, CProxy_TreePiece<Data> &treepieces,
      std::vector<Particle> &particles, std::vector<Splitter> &splitters) {
    // TODO SFC decomposition
    // Probably need to use prefix sum
    int flush_count;
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

      if (n_particles_left == 0) break;
    }
  }

  static void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) {
    for (unsigned int i = 0; i < particles.size(); i++) {
      particles[i].key = SFC::generateKey(particles[i].position, universe.box);
      // Add placeholder bit
      particles[i].key |= (Key)1 << (KEY_BITS-1);
    }
  }

  static int getNumExpectedParticles(int n_total_particles, int n_treepieces,
      int tp_index, const std::vector<Splitter> &splitters) {
    int n_expected = n_total_particles / n_treepieces;
    if (tp_index < (n_total_particles % n_treepieces)) n_expected++;
    return n_expected;
  }
};

class OctDecomposition {
public:

  static int getNumExpectedParticles(int n_total_particles, int n_treepieces,
      int tp_index, const std::vector<Splitter> &splitters) {
    return splitters[tp_index].n_particles;
  }

  template <typename Data>
  static int flush(int n_total_particles, int n_treepieces, CProxy_TreePiece<Data> &treepieces,
      std::vector<Particle> &particles, std::vector<Splitter> &splitters) {
    // OCT decomposition
    int flush_count = 0;
    int start = 0;
    int finish = particles.size();

    // Find particles that belong to each splitter range and flush them
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

    // Free splitter memory
    splitters.clear();

    return flush_count;
  }

  static void assignKeys(BoundingBox &universe, std::vector<Particle> &particles) {
    SfcDecomposition::assignKeys(universe, particles);
    // Sort particles for decomposition
    std::sort(particles.begin(), particles.end());
  }

  static int findSplitters(BoundingBox &universe, CProxy_Reader &readers, std::vector<Splitter> &splitters) {
    BufferedVec<Key> keys;

    // Initial splitter keys (first and last)
    keys.add(Key(1)); // 0000...1
    keys.add(~Key(0)); // 1111...1
    keys.buffer();

    int decomp_particle_sum = 0; // Used to check if all particles are decomposed

    // Main decomposition loop
    while (keys.size() != 0) {
      // Send splitters to Readers for histogramming
      CkReductionMsg *msg;
      readers.countOct(keys.get(), CkCallbackResumeThread((void*&)msg));
      int* counts = (int*)msg->getData();
      int n_counts = msg->getSize() / sizeof(int);

      // Check counts and create splitters if necessary
      Real threshold = (DECOMP_TOLERANCE * Real(max_particles_per_tp));
      for (int i = 0; i < n_counts; i++) {
        Key from = keys.get(2*i);
        Key to = keys.get(2*i+1);

        int n_particles = counts[i];
        if ((Real)n_particles > threshold) {
          // Create 8 more splitter key pairs to go one level deeper.
          // Leading zeros will be removed in Reader::count() to enable
          // comparison of splitter key and particle key
          keys.add(from << 3);
          keys.add((from << 3) + 1);

          keys.add((from << 3) + 1);
          keys.add((from << 3) + 2);

          keys.add((from << 3) + 2);
          keys.add((from << 3) + 3);

          keys.add((from << 3) + 3);
          keys.add((from << 3) + 4);

          keys.add((from << 3) + 4);
          keys.add((from << 3) + 5);

          keys.add((from << 3) + 5);
          keys.add((from << 3) + 6);

          keys.add((from << 3) + 6);
          keys.add((from << 3) + 7);

          keys.add((from << 3) + 7);
          if (to == (~Key(0)))
            keys.add(~Key(0));
          else
            keys.add(to << 3);
        }
        else {
          // Create and store splitter
          Splitter sp(Utility::removeLeadingZeros(from),
              Utility::removeLeadingZeros(to), from, n_particles);
          splitters.push_back(sp);

          // Add up number of particles to check if all are flushed
          decomp_particle_sum += n_particles;
        }
      }

      keys.buffer();
      delete msg;
    }

    // Check if decomposition is correct
    if (decomp_particle_sum != universe.n_particles) {
      CkAbort("Decomposition failure: only %d particles out of %d decomposed",
          decomp_particle_sum, universe.n_particles);
    }

    // Return the number of TreePieces
    return splitters.size();
  }
};

class OctTree {
public:
  using SendProxyFn = std::function<void(int,int)>;

  static void buildCanopy(const std::vector<Splitter> &splitters, int tp_index, const SendProxyFn &fn) {
    Key tp_key = splitters[tp_index].tp_key;
    Key temp_key = tp_key;
    fn(tp_key, tp_index);
    while (temp_key > 0 && temp_key % BRANCH_FACTOR == 0) {
      temp_key /= BRANCH_FACTOR;
      fn(temp_key, -1);
    }
  }
};

#endif
