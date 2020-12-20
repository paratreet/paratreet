#include <memory>
#include <algorithm>

#include "common.h"
#include "Decomposition.h"
#include "BufferedVec.h"
#include "Reader.h"

int SfcDecomposition::flush(std::vector<Particle> &particles, const SendParticlesFn &fn) {
  int flush_count = 0;
  std::function<bool(const Particle&, Key)> compGE = [this] (const Particle& a, Key b) {return a.key >= b;};
  std::function<bool(const Particle&, Key)> compG  = [this] (const Particle& a, Key b) {return a.key > b;};
  int particle_idx = Utility::binarySearchComp(
    splitters[0].from, particles.data(), 0, particles.size(), compGE
    );
  for (int i = 0; i < splitters.size(); ++i) {
    int end = Utility::binarySearchComp(
      splitters[i].to, particles.data(), particle_idx, particles.size(), compG
      );
    int n_particles = end - particle_idx;
    flush_count += n_particles;
    if (n_particles) fn(i, n_particles, &particles[particle_idx]);
    particle_idx = end;
  }
  return flush_count;
}

void SfcDecomposition::assignKeys(BoundingBox &universe, std::vector<Particle> &particles) {
  for (unsigned int i = 0; i < particles.size(); i++) {
    particles[i].key = SFC::generateKey(particles[i].position, universe.box);
    // Add placeholder bit
    particles[i].key |= (Key)1 << (KEY_BITS-1);
  }
  std::sort(particles.begin(), particles.end());
}

int SfcDecomposition::getNumExpectedParticles(int n_total_particles, int n_partitions,
                                              int tp_index) {
  int n_expected = n_total_particles / n_partitions;
  if (tp_index < (n_total_particles % n_partitions)) n_expected++;
  return n_expected;
}

int SfcDecomposition::findSplitters(BoundingBox &universe, CProxy_Reader &readers, const paratreet::Configuration& config, int log_branch_factor) {
  // countSfc finds the keys of all particles
  CkReductionMsg *msg;
  readers.countSfc(CkCallbackResumeThread((void*&)msg));
  std::vector<Key> keys;
  CkReduction::setElement *elem = (CkReduction::setElement *)msg->getData();
  while (elem != NULL) {
    Key *elemKeys = (Key *)&elem->data;
    int n_keys = elem->dataSize / sizeof(Key);
    keys.insert(keys.end(), elemKeys, elemKeys + n_keys);
    elem = elem->next();
  }

  std::sort(keys.begin(), keys.end());

  int decomp_particle_sum = 0;

  Real threshold = DECOMP_TOLERANCE * Real(config.max_particles_per_tp);
  for (int i = 0; i * threshold < keys.size(); ++i) {
    Key from = keys[(int)(i * threshold)];
    Key to;
    int n_particles = (int)threshold;
    if ((i + 1) * threshold >= keys.size()) {
      to = ~Key(0);
      n_particles = keys.size() - (int)(i * threshold);
    } else to = keys[(int)((i + 1) * threshold)];

    // Inverse bitwise-xor is used as a bitwise equality test, in conjunction with
    // `removeTrailingBits` forms a mask that zeroes off all bits after the first
    // differing bit between `from` and `to`
    Key prefixMask = Utility::removeTrailingBits(~(from ^ (to - 1)));
    Key prefix = prefixMask & from;
    Splitter sp(Utility::removeLeadingZeros(from, log_branch_factor),
                Utility::removeLeadingZeros(to, log_branch_factor), prefix, n_particles);
    splitters.push_back(sp);

    decomp_particle_sum += n_particles;
  }

  // Check if decomposition is correct
  if (decomp_particle_sum != universe.n_particles) {
    CkPrintf("SFC Decomposition failure: only %d particles out of %d decomposed",
             decomp_particle_sum, universe.n_particles);
    CkAbort("SFC Decomposition failure -- see stdout");
  }

  // Sort our splitters
  std::sort(splitters.begin(), splitters.end());

  // Return the number of TreePieces
  return splitters.size();
}

std::vector<Splitter> SfcDecomposition::getSplitters() { return splitters; }

void SfcDecomposition::alignSplitters(SfcDecomposition *decomp)
{
  std::vector<Splitter> target_splitters = decomp->getSplitters();
  splitters[0].from = target_splitters[0].from;
  int target_idx = 1;
  std::function<bool(const Splitter&, Key)> compGE = [this] (const Splitter& a, Key b) {return a.from >= b;};
  for (int i = 1; i < splitters.size(); ++i) {
    target_idx = Utility::binarySearchComp(
      splitters[i].from, target_splitters.data(), target_idx, target_splitters.size(), compGE
      );
    if (splitters[i].from == target_splitters[i].from) { // splitter is already aligned
      if (target_idx < target_splitters.size()) ++target_idx;
      continue;
    }
    if (
      splitters[i].from - target_splitters[target_idx - 1].from <
      target_splitters[target_idx].from - splitters[i].from
      ) --target_idx; // previous splitter is closer

    splitters[i].from = target_splitters[target_idx].from;
    splitters[i - 1].to = splitters[i].from;
    if (target_idx < target_splitters.size()) ++target_idx;
  }
}

void SfcDecomposition::pup(PUP::er& p) {
  PUP::able::pup(p);
  p | splitters;
}

Key SfcDecomposition::getTpKey(int idx) {
  return splitters[idx].tp_key;
}

int OctDecomposition::getNumExpectedParticles(int n_total_particles, int n_partitions,
                                              int tp_index) {
  return splitters[tp_index].n_particles;
}

int OctDecomposition::flush(std::vector<Particle> &particles, const SendParticlesFn &fn) {
  // OCT decomposition
  int flush_count = 0;
  int start = 0;
  int finish = particles.size();

  // Find particles that belong to each splitter range and flush them
  std::function<bool(const Particle&, Key)> compGE = [this] (const Particle& a, Key b) {return a.key >= b;};
  for (int i = 0; i < splitters.size(); i++) {
    int begin = Utility::binarySearchComp(splitters[i].from, &particles[0], start, finish, compGE);
    int end = Utility::binarySearchComp(splitters[i].to, &particles[0], begin, finish, compGE);

    int n_particles = end - begin;

    if (n_particles > 0) {
      fn(i, n_particles, &particles[begin]);
      flush_count += n_particles;
    }

    start = end;
  }

  // Free splitter memory
  return flush_count;
}

void OctDecomposition::assignKeys(BoundingBox &universe, std::vector<Particle> &particles) {
  SfcDecomposition::assignKeys(universe, particles);
  // Sort particles for decomposition
  // std::sort(particles.begin(), particles.end());
}

int OctDecomposition::findSplitters(BoundingBox &universe, CProxy_Reader &readers, const paratreet::Configuration& config, int log_branch_factor) {
  BufferedVec<Key> keys;
  const int branch_factor = (1 << log_branch_factor);

  // Initial splitter keys (first and last)
  keys.add(Key(1)); // 0000...1
  keys.add(~Key(0)); // 1111...1
  keys.buffer();

  int decomp_particle_sum = 0; // Used to check if all particles are decomposed

  // Main decomposition loop
  while (keys.size() != 0) {
    // Send splitters to Readers for histogramming
    CkReductionMsg *msg;
    readers.countOct(keys.get(), log_branch_factor, CkCallbackResumeThread((void*&)msg));
    int* counts = (int*)msg->getData();
    int n_counts = msg->getSize() / sizeof(int);

    // Check counts and create splitters if necessary
    Real threshold = (DECOMP_TOLERANCE * Real(config.max_particles_per_tp));
    for (int i = 0; i < n_counts; i++) {
      Key from = keys.get(2*i);
      Key to = keys.get(2*i+1);

      int n_particles = counts[i];
      if ((Real)n_particles > threshold) {
        // Create *branch_factor* more splitter key pairs to go one level deeper.
        // Leading zeros will be removed in Reader::count() to enable
        // comparison of splitter key and particle key

        for (int k = 0; k < branch_factor; k++) {
          // Add first key in pair
          keys.add(from * branch_factor + k);

          // Add second key in pair
          if (k < branch_factor - 1) {
            keys.add(from * branch_factor + k + 1);
          } else {
            // Clamp to largest key if shifted key is larger
            if (to == (~Key(0))) keys.add(~Key(0));
            else keys.add(to * branch_factor);
          }
        }
      }
      else {
        // Create and store splitter
        Splitter sp(Utility::removeLeadingZeros(from, log_branch_factor),
            Utility::removeLeadingZeros(to, log_branch_factor), from, n_particles);
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
    CkPrintf("Decomposition failure: only %d particles out of %d decomposed",
             decomp_particle_sum, universe.n_particles);
    CkAbort("Decomposition failure -- see stdout");
  }

  // Sort our splitters
  std::sort(splitters.begin(), splitters.end());

  // Return the number of TreePieces
  return splitters.size();
}

Key KdDecomposition::getTpKey(int idx) {
  CkAbort("not implemented yet");
  return 0;
}

int KdDecomposition::flush(std::vector<Particle> &particles, const SendParticlesFn &fn) {
  CkAbort("not implemented yet");
  return 0;
}

void KdDecomposition::assignKeys(BoundingBox &universe, std::vector<Particle> &particles) {
  CkAbort("not implemented yet");
}

int KdDecomposition::getNumExpectedParticles(int n_total_particles, int n_partitions,
                                              int tp_index) {
  return 0;
}

int KdDecomposition::findSplitters(BoundingBox &universe, CProxy_Reader &readers, const paratreet::Configuration& config, int log_branch_factor) {
  CkAbort("not implemented yet");
  return 0;
  // countSfc finds the keys of all particles
}

void KdDecomposition::pup(PUP::er& p) {
  PUP::able::pup(p);
}
