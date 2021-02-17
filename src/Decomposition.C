#include <memory>
#include <algorithm>

#include "common.h"
#include "Decomposition.h"
#include "BufferedVec.h"
#include "Reader.h"

DecompArrayMap::DecompArrayMap(Decomposition* decomp, int n_total_particles, int n_splitters) {
  int threshold = n_total_particles / CkNumPes();
  int current_sum = 0;
  int current_pe = 0;
  int pe_sum = threshold;
  for (int i = 0; i < n_splitters; i++) {
    current_sum += decomp->getNumParticles(i);
    if (current_sum > pe_sum) {
      pe_intervals.push_back(i - 1);
      current_pe ++;
      pe_sum += threshold;
    }
  }
  pe_intervals.push_back(n_splitters - 1);
}

int DecompArrayMap::procNum(int, const CkArrayIndex &idx) {
  int index = *(int *)idx.data();
  if (pe_intervals.empty()) CkAbort("using procNum but array is not prepped");
  auto it = std::lower_bound(pe_intervals.begin(), pe_intervals.end(), index);
  int pe = std::distance(pe_intervals.begin(), it);
  return std::min(CkNumPes() - 1, pe);
}

void Decomposition::assignKeys(BoundingBox &universe, std::vector<Particle> &particles) {
  for (auto & particle : particles) {
    particle.key = SFC::generateKey(particle.position, universe.box);
    // Add placeholder bit
    particle.key |= (Key)1 << (KEY_BITS-1);
  }
}

int SfcDecomposition::flush(std::vector<Particle> &particles, const SendParticlesFn &fn) {
  int flush_count = 0;
  std::function<bool(const Particle&, Key)> compGE = [] (const Particle& a, Key b) {return a.key >= b;};
  std::function<bool(const Particle&, Key)> compG  = [] (const Particle& a, Key b) {return a.key > b;};
  std::sort(particles.begin(), particles.end());
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

int SfcDecomposition::getNumParticles(int tp_index) {
  return splitters[tp_index].n_particles;
}

int SfcDecomposition::findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) {
  return parallelFindSplitters(universe, readers, min_n_splitters);
}

int SfcDecomposition::parallelFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) {
  const int branch_factor = treespec.ckLocalBranch()->getTree()->getBranchFactor();
  const int log_branch_factor = log2(branch_factor);

  readers.localSort(CkCallbackResumeThread());
  std::vector<QuickSelectSFCState> states (min_n_splitters);
  int ki = 0;
  saved_n_total_particles = universe.n_particles;
  int threshold = saved_n_total_particles / min_n_splitters;
  int remainder = saved_n_total_particles % min_n_splitters;
  for (size_t i = 0u; i < states.size(); i++) {
    states[i].start_range = Utility::removeLeadingZeros(Key(1), log_branch_factor);
    states[i].goal_rank = ki + threshold;
    if (i < remainder) states[i].goal_rank++;
    ki = states[i].goal_rank;
#if DEBUG
    CkPrintf("goal rank %d is %d\n", i, ki);
#endif
  }

  int n_pending = states.size();
  while (n_pending > 0) {
    CkReductionMsg *msg;
    readers.countSfc(states, log_branch_factor, CkCallbackResumeThread((void*&)msg));
    int* counts = (int*)msg->getData();
    for (int i = 0; i < states.size(); i++) {
      auto&& count = counts[i];
      auto&& state = states[i];
      if (!state.pending) continue;
#if DEBUG
      CkPrintf("count %d is %d for start_range %" PRIx64 " end_range %" PRIx64 " compare_to %" PRIx64 "\n", i, counts[i], state.start_range, state.end_range, state.compare_to());
#endif
      if (count < state.goal_rank) {
        state.start_range = state.compare_to();
        state.goal_rank -= count;
      }
      else if (count > state.goal_rank) {
         state.end_range = state.compare_to();
      }
      else {
        state.pending = false;
        n_pending--;
      }
    }
  }
  Key from (0), to;
  int prev = 0;
  for (auto && state : states) {
    to = state.compare_to();
    Key prefixMask = Utility::removeTrailingBits(~(from ^ (to - 1)));
    Key prefix = prefixMask & from;
    Splitter sp(Utility::removeLeadingZeros(from, log_branch_factor),
                Utility::removeLeadingZeros(to, log_branch_factor), prefix, state.goal_rank - prev);
    splitters.push_back(sp);
    prev = state.goal_rank;
    from = to;
  }
  return splitters.size();
}

int SfcDecomposition::serialFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) {
  const int branch_factor = treespec.ckLocalBranch()->getTree()->getBranchFactor();
  const int log_branch_factor = log2(branch_factor);
  CkReductionMsg *msg;
  readers.getAllSfcKeys(CkCallbackResumeThread((void*&)msg));
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

  saved_n_total_particles = universe.n_particles;
  int threshold = saved_n_total_particles / min_n_splitters;
  int remainder = saved_n_total_particles % min_n_splitters;
  for (int i = 0, ki = 0; i < min_n_splitters; ++i) {
    Key from = keys[ki], to;
    int ki_next = ki + threshold;
    if (i < remainder) ki_next++;
    if (ki_next >= saved_n_total_particles) to = ~Key(0);
    else to = keys[ki_next];
    // Inverse bitwise-xor is used as a bitwise equality test, in conjunction with
    // `removeTrailingBits` forms a mask that zeroes off all bits after the first
    // differing bit between `from` and `to`
    Key prefixMask = Utility::removeTrailingBits(~(from ^ (to - 1)));
    Key prefix = prefixMask & from;
    Splitter sp(Utility::removeLeadingZeros(from, log_branch_factor),
                Utility::removeLeadingZeros(to, log_branch_factor), prefix, ki_next - ki);
    splitters.push_back(sp);
    decomp_particle_sum += (ki_next - ki);
    ki = ki_next;
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
  std::function<bool(const Splitter&, Key)> compGE = [] (const Splitter& a, Key b) {return a.from >= b;};
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
  p | saved_n_total_particles;
}

Key SfcDecomposition::getTpKey(int idx) {
  return splitters[idx].tp_key;
}

void OctDecomposition::setArrayOpts(CkArrayOptions& opts) {
  auto myMap = CProxy_DecompArrayMap::ckNew(this, saved_n_total_particles, splitters.size());
  opts.setMap(myMap);
}

int OctDecomposition::flush(std::vector<Particle> &particles, const SendParticlesFn &fn) {
  // OCT decomposition
  int flush_count = 0;
  int start = 0;
  int finish = particles.size();

  // Find particles that belong to each splitter range and flush them
  std::function<bool(const Particle&, Key)> compGE = [] (const Particle& a, Key b) {return a.key >= b;};
  std::sort(particles.begin(), particles.end());
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

int OctDecomposition::findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) {
  const int branch_factor = treespec.ckLocalBranch()->getTree()->getBranchFactor();
  const int log_branch_factor = log2(branch_factor);

  BufferedVec<Key> keys;
  // Initial splitter keys (first and last)
  keys.add(Key(1)); // 0000...1
  keys.add(~Key(0)); // 1111...1
  keys.buffer();

  readers.localSort(CkCallbackResumeThread());
  int decomp_particle_sum = 0; // Used to check if all particles are decomposed
  int threshold = universe.n_particles / min_n_splitters;
  // Main decomposition loop
  while (keys.size() != 0) {
    // Send splitters to Readers for histogramming
    CkReductionMsg *msg;
    readers.countOct(keys.get(), log_branch_factor, CkCallbackResumeThread((void*&)msg));
    int* counts = (int*)msg->getData();
    int n_counts = msg->getSize() / sizeof(int);
    // Check counts and create splitters if necessary
    for (int i = 0; i < n_counts; i++) {
      Key from = keys.get(2*i);
      Key to = keys.get(2*i+1);

      int n_particles = counts[i];
      if (n_particles > threshold) {
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
  saved_n_total_particles = universe.n_particles;

  // Return the number of TreePieces
  return splitters.size();
}

Key BinaryDecomposition::getTpKey(int idx) {
  return idx + (1 << depth);
}

int BinaryDecomposition::flush(std::vector<Particle> &particles, const SendParticlesFn &fn) {
  std::vector<std::vector<Particle>> out_particles (1 << depth);
  for (auto && particle : particles) {
    int index = 1;
    for (int cdepth = 0; cdepth < depth; cdepth++) {
      if (particle.position[splitters[index].first] > splitters[index].second) {
        index = index * 2 + 1;
      }
      else {
        index = index * 2;
      }
    }
    auto part_index = index - (1 << depth);
    out_particles[part_index].push_back(particle);
  }
  particles.clear();
  for (int idx = 0; idx < out_particles.size(); idx++) {
    auto && out = out_particles[idx];
    if (!out.empty()) fn(idx, out.size(), out.data());
    particles.insert(particles.begin(), out.begin(), out.end());
  }
  return particles.size();
}

int BinaryDecomposition::getNumParticles(int tp_index) {
  return bins_sizes[tp_index];
}

int BinaryDecomposition::findSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) {
  return parallelFindSplitters(universe, readers, min_n_splitters);
}

int BinaryDecomposition::parallelFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) {
  bins_sizes = std::vector<int>(1, universe.n_particles);
  splitters.emplace_back(0, 0); // empty space for key=0
  saved_n_total_particles = universe.n_particles;
  for (; (1 << depth) < min_n_splitters; depth++) {
    auto && level_splitters = this->sortAndGetSplitters(universe, readers);
    CkReductionMsg *msg;
    readers.doBinarySplit(level_splitters, CkCallbackResumeThread((void*&)msg));
    int* counts = (int*)msg->getData();
    bins_sizes.clear();
    bins_sizes.resize(2 * level_splitters.size());
    for (int i = 0; i < bins_sizes.size(); i++) {
      bins_sizes[i] = counts[i];
    }
    splitters.insert(splitters.end(), level_splitters.begin(), level_splitters.end());
  }
  return (1 << depth);
}

int BinaryDecomposition::serialFindSplitters(BoundingBox &universe, CProxy_Reader &readers, int min_n_splitters) {
  CkReductionMsg *msg;
  readers.getAllPositions(CkCallbackResumeThread((void*&)msg));
  std::vector<Bin> bins (1);
  CkReduction::setElement *elem = (CkReduction::setElement *)msg->getData();
  while (elem != NULL) {
    Vector3D<Real> *elemPos = (Vector3D<Real> *)&elem->data;
    int n_pos = elem->dataSize / sizeof(Vector3D<Real>);
    bins.front().insert(bins.front().end(), elemPos, elemPos + n_pos);
    elem = elem->next();
  }

  saved_n_total_particles = universe.n_particles;
  splitters.emplace_back(0, 0); // empty space for key=0
  for (; (1 << depth) < min_n_splitters; depth++) {
    decltype(bins) binsCopy (bins.size() * 2);
    for (size_t i = 0u; i < bins.size(); i++) {
      auto& bin = bins[i];
      auto split = this->sortAndGetSplitter(depth, bin);
#if DEBUG
      std::cout << "splitting at depth " << depth << " on index " << splitters.size() << " with dim ";
      std::cout << split.first << " at " << split.second << std::endl;
#endif
      splitters.push_back(split);
      this->assign(bin, binsCopy[2 * i], binsCopy[2 * i + 1], split);
    }
    std::swap(bins, binsCopy);
  }
  for (auto && bin : bins) {
    bins_sizes.push_back(bin.size());
  }
  return (1 << depth);
}

void BinaryDecomposition::pup(PUP::er& p) {
  p | splitters;
  p | depth;
  p | saved_n_total_particles;
  p | bins_sizes;
}

void KdDecomposition::assign(Bin& parent, Bin& left, Bin& right, BinarySplit split) {
  size_t medianIndex = (parent.size() + 1) / 2;
  left.insert(left.end(), parent.begin(), parent.begin() + medianIndex);
  right.insert(right.end(), parent.begin() + medianIndex, parent.end());
}

std::vector<BinaryDecomposition::BinarySplit> KdDecomposition::sortAndGetSplitters(BoundingBox &universe, CProxy_Reader &readers) {
  std::vector<QuickSelectKDState> states (bins_sizes.size());
  for (int i = 0; i < states.size(); i++) {
    auto && state = states[i];
    state.dim = (depth % NDIM);
    state.start_range = universe.box.lesser_corner[state.dim];
    state.end_range  = universe.box.greater_corner[state.dim];
    state.goal_rank = bins_sizes[i] / 2 + (bins_sizes[i] % 2); // left heavy
  }
  int n_pending = states.size();
  while (n_pending > 0) {
    CkReductionMsg *msg;
    readers.countKd(states, CkCallbackResumeThread((void*&)msg));
    int* counts = (int*)msg->getData();
    for (int i = 0; i < states.size(); i++) {
      auto&& count = counts[i];
      auto&& state = states[i];
      if (!state.pending) continue;
#if DEBUG
      CkPrintf("count %d is %d for goal_rank %d start_range %f end_range %f compare_to %f\n", i, counts[i], state.goal_rank, state.start_range, state.end_range, state.compare_to());
#endif
      if (count == state.goal_rank || state.start_range == state.end_range) {
        state.pending = false;
        n_pending--;
      }
      else if (count < state.goal_rank) {
        state.start_range = state.compare_to();
        state.goal_rank -= count;
      }
      else {
         state.end_range = state.compare_to();
      }
    }
  }
  std::vector<BinarySplit> level_splitters;
  for (auto && state : states) level_splitters.emplace_back(state.dim, state.compare_to());
  return level_splitters;
}

std::pair<int, Real> KdDecomposition::sortAndGetSplitter(int depth, Bin& bin) {
  static auto compX = [] (const Vector3D<Real>& a, const Vector3D<Real>& b) {return a.x < b.x;};
  static auto compY = [] (const Vector3D<Real>& a, const Vector3D<Real>& b) {return a.y < b.y;};
  static auto compZ = [] (const Vector3D<Real>& a, const Vector3D<Real>& b) {return a.z < b.z;};
  int dim = depth % NDIM;
  if (dim == 0)      std::sort(bin.begin(), bin.end(), compX);
  else if (dim == 1) std::sort(bin.begin(), bin.end(), compY);
  else if (dim == 2) std::sort(bin.begin(), bin.end(), compZ);
  size_t medianIndex = (bin.size() + 1) / 2;
  auto median = bin[medianIndex][dim]; // not average?
  return {dim, median};
}

void KdDecomposition::pup(PUP::er& p) {
  PUP::able::pup(p);
  BinaryDecomposition::pup(p);
}

void LongestDimDecomposition::assign(Bin& parent, Bin& left, Bin& right, BinarySplit split) {
  for (auto && pos : parent) {
    if (pos[split.first] > split.second) right.push_back(pos);
    else left.push_back(pos);
  }
}

std::pair<int, Real> LongestDimDecomposition::sortAndGetSplitter(int depth, Bin& bin) {
  OrientedBox<Real> box;
  Vector3D<Real> unweighted_center;
  for (auto && pos : bin) {
    box.grow(pos);
    unweighted_center += pos;
  }
  unweighted_center /= bin.size();
  auto dims = box.greater_corner - box.lesser_corner;
  int best_dim = 0;
  Real max_dim = 0;
  for (int d = 0; d < NDIM; d++) {
    if (dims[d] > max_dim) {
      max_dim = dims[d];
      best_dim = d;
    }
  }
  return {best_dim, unweighted_center[best_dim]};
}

std::vector<BinaryDecomposition::BinarySplit> LongestDimDecomposition::sortAndGetSplitters(BoundingBox &universe, CProxy_Reader &readers) {
  CkReductionMsg *msg;
  readers.countLongestDim(CkCallbackResumeThread((void*&)msg));
  CkReduction::tupleElement* res = nullptr;
  int numRedn = 0;
  msg->toTuple(&res, &numRedn);
  Vector3D<Real>* centers = (Vector3D<Real>*)(res[0].data);
  int* counts = (int*)(res[1].data);
  Vector3D<Real>* lesser_corners = (Vector3D<Real>*)(res[2].data);
  Vector3D<Real>* greater_corners = (Vector3D<Real>*)(res[3].data);

  std::vector<BinarySplit> level_splitters;
  int n_bins = (1 << depth);
  for (int i = 0; i < n_bins; i++) {
    auto dims = greater_corners[i] - lesser_corners[i];
    int best_dim = 0;
    Real max_dim = 0;
    for (int d = 0; d < NDIM; d++) {
      if (dims[d] > max_dim) {
        max_dim = dims[d];
        best_dim = d;
      }
    }
    auto unweighted_center = centers[i] / counts[i];
    level_splitters.emplace_back(best_dim, unweighted_center[best_dim]);
  }
  return level_splitters;
}

void LongestDimDecomposition::pup(PUP::er& p) {
  PUP::able::pup(p);
  BinaryDecomposition::pup(p);
}

void LongestDimDecomposition::setArrayOpts(CkArrayOptions& opts) {
  auto myMap = CProxy_DecompArrayMap::ckNew(this, saved_n_total_particles, splitters.size());
  opts.setMap(myMap);
}
