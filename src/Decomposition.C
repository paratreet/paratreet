#include <memory>
#include <algorithm>

#include "common.h"
#include "Decomposition.h"
#include "BufferedVec.h"
#include "Reader.h"

extern int max_particles_per_tp; // for OCT decomposition

int SfcDecomposition::flush(int n_total_particles, int n_treepieces, const SendParticlesFn &fn,
                            std::vector<Particle> &particles) {
  int flush_count = 0;
  int particle_idx = Utility::binarySearchGE(
    splitters[0].from, particles.data(), 0, particles.size()
    );
  for (int i = 0; i < splitters.size(); ++i) {
    int end = Utility::binarySearchG(
      splitters[i].to, particles.data(), particle_idx, particles.size()
      );
    int n_particles = end - particle_idx;
    flush_count += n_particles;
    if (n_particles) fn(i, n_particles, &particles[particle_idx]);
    particle_idx = end;
  }
  splitters.clear();
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

int SfcDecomposition::getNumExpectedParticles(int n_total_particles, int n_treepieces,
                                              int tp_index) {
  int n_expected = n_total_particles / n_treepieces;
  if (tp_index < (n_total_particles % n_treepieces)) n_expected++;
  return n_expected;
}

int SfcDecomposition::findSplitters(BoundingBox &universe, CProxy_Reader &readers) {
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

  Real threshold = DECOMP_TOLERANCE * Real(max_particles_per_tp);
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
    Splitter sp(Utility::removeLeadingZeros(from),
                Utility::removeLeadingZeros(to), prefix, n_particles);
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

void SfcDecomposition::alignSplitters(Decomposition *decomp)
{
  std::vector<Splitter> target_splitters = decomp->getSplitters();
  splitters[0].from = target_splitters[0].from;
  int target_idx = 1;
  for (int i = 1; i < splitters.size(); ++i, ++target_idx) {
    target_idx = Utility::binarySearchGE(
      splitters[i], target_splitters.data(), target_idx, target_splitters.size()
      );
    if (splitters[i].from == target_splitters[i].from) // splitter is already aligned
      continue;

    splitters[i].from = target_splitters[target_idx].from;
    splitters[i - 1].to = splitters[i].from;
  }
}

void SfcDecomposition::pup(PUP::er& p) {
  p | splitters;
}

int SfcDecomposition::getTpKey(int idx) {
  return splitters[idx].tp_key;
}

int OctDecomposition::getNumExpectedParticles(int n_total_particles, int n_treepieces,
                                              int tp_index) {
  return splitters[tp_index].n_particles;
}

int OctDecomposition::flush(int n_total_particles, int n_treepieces, const SendParticlesFn &fn,
                            std::vector<Particle> &particles) {
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
      fn(i, n_particles, &particles[begin]);
      flush_count += n_particles;
    }

    start = end;
  }

  // Free splitter memory
  splitters.clear();

  return flush_count;
}

void OctDecomposition::assignKeys(BoundingBox &universe, std::vector<Particle> &particles) {
  SfcDecomposition::assignKeys(universe, particles);
  // Sort particles for decomposition
  // std::sort(particles.begin(), particles.end());
}

int OctDecomposition::findSplitters(BoundingBox &universe, CProxy_Reader &readers) {
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
    CkPrintf("Decomposition failure: only %d particles out of %d decomposed",
             decomp_particle_sum, universe.n_particles);
    CkAbort("Decomposition failure -- see stdout");
  }

  // Sort our splitters
  std::sort(splitters.begin(), splitters.end());

  // Return the number of TreePieces
  return splitters.size();
}
