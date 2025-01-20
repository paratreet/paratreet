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

CollocateMap::CollocateMap(Decomposition* d, const std::vector<int>& pl)
  : partition_locations(pl),
    decomp(d)
{
}

int CollocateMap::procNum(int, const CkArrayIndex &idx) {
  int index = *(int *)idx.data();
  size_t p_home = decomp->getPartitionHome(index);
  CkAssert(p_home < partition_locations.size());
  return partition_locations[p_home];
}

void Decomposition::pup(PUP::er& p) {
  PUP::able::pup(p);
}

void Decomposition::setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate) {
  if (collocate) {
    auto myMap = CProxy_CollocateMap::ckNew(CkPointer<Decomposition>(this), partition_locations);
    opts.setMap(myMap);
  }
}

CkReductionMsg* Decomposition::doSplit(const std::vector<GenericSplitter>& splits) {
  return nullptr;
}

bool SfcDecomposition::flush(IParticleViewer* particles, int* destinations) {
  std::function<bool(const IParticleViewer*, int, Key)> compGE = [] (const IParticleViewer* particles, int a, Key b) {return particles->key(a) >= b;};
  std::function<bool(const IParticleViewer*, int, Key)> compG  = [] (const IParticleViewer* particles, int a, Key b) {return particles->key(a) > b;};
  particles->sortByKey([] (auto && a, auto && b) {return a < b;});
  int begin = Utility::binarySearchComp(
    splitters[0].from, particles, 0, particles->size(), compGE
    );
  for (int i = 0; i < splitters.size(); ++i) {
    int end = Utility::binarySearchComp(
      splitters[i].to, particles, begin, particles->size(), compG
    );
    for (size_t pi = begin; pi < end; pi++) {
      destinations[pi] = i;
    }
    begin = end;
  }
  return true; // sorted
}

int SfcDecomposition::getNumParticles(int tp_index) {
  return splitters[tp_index].n_particles;
}

int SfcDecomposition::getPartitionHome(int tp_index) {
  return partition_idxs[tp_index];
}

// called by each reader
// inputs: splitters and particles
// assumed state: none
// state change: none
// outputs: count array if doing that split. size = states.size()
CkReductionMsg* SfcDecomposition::countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) {
  std::vector<int> counts (states.size(), 0);
  std::function<bool(const IParticleViewer*, int, Key)> compGE = [] (const IParticleViewer* particles, int a, Key b) {return particles->key(a) >= b;};
  if (particles->size() > 0) {
    for (size_t i = 0u; i < states.size(); i++) {
      if (!weight_by_partition && !states[i].pending) continue;
      int begin = Utility::binarySearchComp(states[i].start_key, particles, 0, particles->size(), compGE);
      int found = Utility::binarySearchComp(states[i].midKey(), particles, begin, particles->size(), compGE);
      if (weight_by_partition) {
        for (int j = begin; j < found; j++) {
	  counts[i] += particles->partitionIndex(j);
	}
      }
      else {
        counts[i] = found - begin;
      }
    }
  }
  return CkReductionMsg::buildNew(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int);
}

int SfcDecomposition::findSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) {
  return parallelFindSplitters(universe, n_total_particles, readers, min_n_splitters, is_subtree);
}

int SfcDecomposition::parallelFindSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) {
  const int branch_factor = treespec.ckLocalBranch()->getTree()->getBranchFactor();
  const int log_branch_factor = log2(branch_factor);

  readers->localSortByKey(CkCallbackResumeThread());
  std::vector<GenericSplitter> states (min_n_splitters);
  int ki = 0;
  saved_n_total_particles = n_total_particles;
  int threshold = saved_n_total_particles / min_n_splitters;
  int remainder = saved_n_total_particles % min_n_splitters;
  for (size_t i = 0u; i < states.size(); i++) {
    states[i].start_key = Utility::removeLeadingZeros(Key(1), log_branch_factor);
    states[i].goal_rank = ki + threshold;
    if (i < remainder) states[i].goal_rank++;
    ki = states[i].goal_rank;
#if DEBUG
    CkPrintf("goal rank %d is %d\n", i, ki);
#endif
  }

  std::vector<int> counts (states.size(), 0);
  int n_pending = states.size();
  while (n_pending > 0) {
    CkReductionMsg *msg;
    readers->countAssignments(states, is_subtree, CkCallbackResumeThread((void*&)msg), false);
    int* temp_counts = (int*)msg->getData();

    for (int i = 0; i < states.size(); i++) {
      auto&& count = temp_counts[i];
      auto&& state = states[i];
      if (!state.pending) continue;
#if DEBUG
      CkPrintf("count %d is %d for start_range %" PRIx64 " end_range %" PRIx64 " compare_to %" PRIx64 "\n", i, counts[i], state.start_key, state.end_key, state.midKey());
#endif
      bool identical = state.end_key - state.start_key <= 1;
      if (count == state.goal_rank || identical) {
        counts[i] = count;
        state.pending = false;
        n_pending--;
      }
      else if (count < state.goal_rank) {
        state.start_key = state.midKey();
        state.goal_rank -= count;
      }
      else {
        state.end_key = state.midKey();
      }
    }
    delete msg;
  }

  CkReductionMsg *msg;
  readers->countAssignments(states, is_subtree, CkCallbackResumeThread((void*&)msg), true);
  int* temp_counts = (int*)msg->getData();
  partition_idxs = {temp_counts, temp_counts + states.size()};
  delete msg;

  Key from (0), to;
  int prev = 0;
  for (size_t i = 0u; i < states.size(); i++) {
    auto& state = states[i];
    to = state.midKey();
    Key prefixMask = Utility::removeTrailingBits(~(from ^ (to - 1)));
    Key prefix = prefixMask & from;
    Splitter sp(Utility::removeLeadingZeros(from, log_branch_factor),
                Utility::removeLeadingZeros(to, log_branch_factor), prefix, counts[i]);
    splitters.push_back(sp);
    prev = state.goal_rank;
    from = to;
  }
  CkAssert(partition_idxs.size() == splitters.size());
  for (int i = 0; i < partition_idxs.size(); i++) {
    if (counts[i] > 0) partition_idxs[i] /= counts[i];
  }
  return splitters.size();
}
/*
int SfcDecomposition::serialFindSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters) {
  const int branch_factor = treespec.ckLocalBranch()->getTree()->getBranchFactor();
  const int log_branch_factor = log2(branch_factor);
  CkReductionMsg *msg;
  readers->getAllSfcKeys(CkCallbackResumeThread((void*&)msg));
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

  saved_n_total_particles = n_total_particles;
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
  if (decomp_particle_sum != n_total_particles) {
    CkPrintf("SFC Decomposition failure: only %d particles out of %d decomposed",
             decomp_particle_sum, n_total_particles);
    CkAbort("SFC Decomposition failure -- see stdout");
  }

  // Sort our splitters
  std::sort(splitters.begin(), splitters.end());

  // Return the number of TreePieces
  return splitters.size();
}
*/
std::vector<Splitter> SfcDecomposition::getSplitters() { return splitters; }

void SfcDecomposition::alignSplitters(SfcDecomposition *decomp)
{
  std::vector<Splitter> target_splitters = decomp->getSplitters();
  splitters[0].from = target_splitters[0].from;
  int target_idx = 1;
  std::function<bool(const Splitter*, int, Key)>  compGE = [] (const Splitter* splitters, int a, Key b) {return splitters[a].from >= b;};
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
  Decomposition::pup(p);
  p | splitters;
  p | partition_idxs;
  p | saved_n_total_particles;
}

Key SfcDecomposition::getTpKey(int idx) {
  return splitters[idx].tp_key;
}

void OctDecomposition::setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate) {
  if (collocate) {
    Decomposition::setArrayOpts(opts, partition_locations, collocate);
  }
  else {
    auto myMap = CProxy_DecompArrayMap::ckNew(this, saved_n_total_particles, splitters.size());
    opts.setMap(myMap);
  }
}

bool OctDecomposition::flush(IParticleViewer* particles, int* destinations) {
  // OCT decomposition
  int start = 0;
  int finish = particles->size();

  // Find particles that belong to each splitter range and flush them
  std::function<bool(const IParticleViewer*, int, Key)> compGE = [] (const IParticleViewer* particles, int a, Key b) {return particles->key(a) >= b;};
  particles->sortByKey([] (auto && a, auto && b) {return a < b;});
  for (int i = 0; i < splitters.size(); i++) {
    int begin = Utility::binarySearchComp(splitters[i].from, particles, start, finish, compGE);
    int end = Utility::binarySearchComp(splitters[i].to, particles, begin, finish, compGE);
    for (size_t pi = begin; pi < end; pi++) {
      destinations[pi] = i;
    }
    start = end;
  }
  return true;
}

// called by each reader
// inputs: splitters and particles
// assumed state: particles are sorted
// state change: none
// outputs: count array if doing that split. size = states.size()
CkReductionMsg* OctDecomposition::countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) {
  std::vector<int> counts (states.size(), 0);
  // Search for the first particle whose key is greater or equal to the input key,
  // in the range [start, finish). This should also work for OCT as the particle
  // keys are SFC keys.
  int start = 0;
  int finish = particles->size();
  Key from, to;
  std::function<bool(const IParticleViewer*, int, Key)> compGE = [] (const IParticleViewer* particles, int a, Key b) {return particles->key(a) >= b;};
  if (particles->size() > 0) {
    for (int i = 0; i < counts.size(); i++) {
      from = states[i].start_key;
      to = states[i].end_key;
      int begin = Utility::binarySearchComp(from, particles, start, finish, compGE);
      int end = Utility::binarySearchComp(to, particles, begin, finish, compGE);
      if (weight_by_partition) {
        for (int j = begin; j < end; j++) {
          counts[i] += particles->partitionIndex(j);
        }
      } else {
        counts[i] = end - begin;
      }
      start = end;
    }
  }
  return CkReductionMsg::buildNew(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int);
}

int OctDecomposition::findSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) {
  const int branch_factor = getBranchFactor();
  const int log_branch_factor = log2(branch_factor);

  BufferedVec<Key> keys;
  // Initial splitter keys (first and last)
  keys.add(Key(1)); // 0000...1
  keys.add(~Key(0)); // 1111...1
  keys.buffer();

  readers->localSortByKey(CkCallbackResumeThread());
  int decomp_particle_sum = 0; // Used to check if all particles are decomposed
  int threshold = n_total_particles / min_n_splitters;
  // Main decomposition loop
  while (keys.size() != 0) {
    // Send splitters to Readers for histogramming
    CkReductionMsg *msg;
    std::vector<GenericSplitter> states;
    for (int i = 0; i < keys.size(); i += 2) {
      states.emplace_back();
      states.back().start_key = Utility::removeLeadingZeros(keys.get(i), log_branch_factor);
      states.back().end_key = Utility::removeLeadingZeros(keys.get(i+1), log_branch_factor);
    }
    readers->countAssignments(states, is_subtree, CkCallbackResumeThread((void*&)msg), false);
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

  std::sort(splitters.begin(), splitters.end());

  {
    std::vector<GenericSplitter> states;
    for (auto& sp : splitters) {
      states.emplace_back();
      states.back().start_key = sp.from;
      states.back().end_key = sp.to;
    }
    CkReductionMsg *msg;
    readers->countAssignments(states, is_subtree, CkCallbackResumeThread((void*&)msg), true);
    int* counts = (int*)msg->getData();
    int n_counts = msg->getSize() / sizeof(int);
    partition_idxs = {counts, counts + n_counts};
    delete msg;
  }
  CkAssert(partition_idxs.size() == splitters.size());
  for (int i = 0; i < partition_idxs.size(); i++) {
    if (splitters[i].n_particles > 0) partition_idxs[i] /= splitters[i].n_particles;
  }

  // Check if decomposition is correct
  if (decomp_particle_sum != n_total_particles) {
    CkPrintf("Decomposition failure: only %d particles out of %d decomposed",
             decomp_particle_sum, n_total_particles);
    CkAbort("Decomposition failure -- see stdout");
  }

  saved_n_total_particles = n_total_particles;

  // Return the number of TreePieces
  return splitters.size();
}

Key BinaryDecomposition::getTpKey(int idx) {
  return idx + (1 << depth);
}

bool BinaryDecomposition::flush(IParticleViewer* particles, int* destinations) {
  for (size_t pi = 0u; pi < particles->size(); pi++) {
    int index = 1;
    auto&& position = particles->position(pi);
    for (int cdepth = 0; cdepth < depth; cdepth++) {
      if (position[splitters[index].dim] > splitters[index].midFloat()) {
        index = index * 2 + 1;
      }
      else {
        index = index * 2;
      }
    }
    destinations[pi] = index - (1 << depth);
  }
  return false;
}

int BinaryDecomposition::getNumParticles(int tp_index) {
  return bins_sizes[tp_index];
}

int BinaryDecomposition::getPartitionHome(int tp_index) {
  return partition_idxs.empty() ? 0 : partition_idxs[tp_index];
}

// called by each reader
// inputs: particles from reader. this is how initial data is passed to Decomposition
// state change: bins becomes a vector-vector of size 1xn_particles
void BinaryDecomposition::initBinarySplit(const IParticleViewer* particles) {
  bins.clear();
  bins.emplace_back();
  for (size_t pi = 0u; pi < particles->size(); pi++) {
    bins.back().emplace_back(particles->partitionIndex(pi), particles->position(pi));
  }
}

int BinaryDecomposition::findSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) {
  return parallelFindSplitters(universe, n_total_particles, readers, min_n_splitters, is_subtree);
}

// called by each reader
// inputs: bottom row of splitters, reader for contribute call.
// assumed state: bins have already been split up until this new row of splitters
// state change: bins gets doubled in size
// outputs: two new count arrays after splitting, which each have size (2 * splits.size())
// first is unweighted, second is weighted
CkReductionMsg* BinaryDecomposition::doSplit(const std::vector<GenericSplitter>& splits) {
  CkAssert(bins.size() == splits.size());
  decltype(bins) binsCopy (2 * bins.size());
  std::vector<int> counts (splits.size() * 4, 0); // trust
  for (int i = 0; i < bins.size(); i++) {
    std::vector<Vector3D<Real>> left, right;
    for (auto && pos : bins[i]) {
      int new_idx = 2 * i;
      if (pos.second[splits[i].dim] > splits[i].midFloat()) new_idx++; // left heavy
      binsCopy[new_idx].push_back(pos);
      counts[new_idx]++;
      counts[new_idx + 2 * bins.size()] += pos.first;
    }
  }
  bins = binsCopy;
  return CkReductionMsg::buildNew(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int);
}

int BinaryDecomposition::parallelFindSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters, bool is_subtree) {
  bins_sizes = std::vector<int>(1, n_total_particles);
  splitters.emplace_back(); // empty space for key=0
  saved_n_total_particles = n_total_particles;
  for (; (1 << depth) < min_n_splitters; depth++) {
    auto && level_splitters = this->sortAndGetSplitters(universe, n_total_particles, readers, is_subtree);
    CkReductionMsg *msg;
    readers->doSplit(level_splitters, is_subtree, CkCallbackResumeThread((void*&)msg));
    int* counts = (int*)msg->getData();
    bins_sizes = std::vector<int> (counts, counts + (2 * level_splitters.size()));
    partition_idxs = std::vector<int>(counts + (2 * level_splitters.size()), counts + (4 * level_splitters.size()));
    splitters.insert(splitters.end(), level_splitters.begin(), level_splitters.end());
  }
  for (int i = 0; i < partition_idxs.size(); i++) {
    if (bins_sizes[i] > 0) partition_idxs[i] /= bins_sizes[i];
  }
  return (1 << depth);
}
/*
int BinaryDecomposition::serialFindSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, int min_n_splitters) {
  CkReductionMsg *msg;
  readers->getAllPositions(CkCallbackResumeThread((void*&)msg));
  std::vector<Bin> bins (1);
  CkReduction::setElement *elem = (CkReduction::setElement *)msg->getData();
  while (elem != NULL) {
    std::pair<int, Vector3D<Real>> *elemPos = (std::pair<int, Vector3D<Real>> *)&elem->data;
    int n_pos = elem->dataSize / sizeof(Vector3D<Real>);
    bins.front().insert(bins.front().end(), elemPos, elemPos + n_pos);
    elem = elem->next();
  }

  saved_n_total_particles = n_total_particles;
  splitters.emplace_back(); // empty space for key=0
  for (; (1 << depth) < min_n_splitters; depth++) {
    decltype(bins) binsCopy (bins.size() * 2);
    for (size_t i = 0u; i < bins.size(); i++) {
      auto& bin = bins[i];
      auto split = this->sortAndGetSplitter(depth, bin);
#if DEBUG
      std::cout << "splitting at depth " << depth << " on index " << splitters.size() << " with dim ";
      std::cout << split.first << " at " << split.second << std::endl;
#endif
      splitters.emplace_back();
      splitters.back().dim = split.first;
      splitters.back().start_float = splitters.back().end_float = split.second;
      this->assign(bin, binsCopy[2 * i], binsCopy[2 * i + 1], split);
    }
    std::swap(bins, binsCopy);
  }
  for (auto && bin : bins) {
    bins_sizes.push_back(bin.size());
    partition_idxs.push_back(bin.empty() ? 0 : bin[0].first);
  }
  return (1 << depth);
}
*/
void BinaryDecomposition::pup(PUP::er& p) {
  Decomposition::pup(p);
  p | splitters;
  p | depth;
  p | saved_n_total_particles;
  p | bins_sizes;
  p | partition_idxs;
}

// this is not used by parallelFindSplitters
void KdDecomposition::assign(Bin& parent, Bin& left, Bin& right, BinarySplit split) {
  size_t medianIndex = (parent.size() + 1) / 2;
  left.insert(left.end(), parent.begin(), parent.begin() + medianIndex);
  right.insert(right.end(), parent.begin() + medianIndex, parent.end());
}

// called by each reader
// inputs: splitters and particles. do not use particles. not used with weight_by_partition
// assumed state: bins has size = splits.size(). so we just check if each particle would go left or right
// state change: none, unless this is the first time we're doing this. then we use initBinarySplit
// outputs: count array if doing that split. size = states.size().
// the sum of these counts is not = n_particles, because a particle either goes left or right
CkReductionMsg* KdDecomposition::countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) {
  std::vector<int> counts (states.size(), 0);
  if (bins.empty()) initBinarySplit(particles);
  for (int i = 0; i < states.size(); i++) {
    auto && state = states[i];
    if (!weight_by_partition && !states[i].pending) continue;
    auto && bin = bins[i];
    for (auto && pos : bin) {
      if (pos.second[state.dim] > state.start_float &&
          pos.second[state.dim] < state.midFloat()) {
        counts[i]++;
      }
    }
  }
  return CkReductionMsg::buildNew(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int);
}

std::vector<GenericSplitter> KdDecomposition::sortAndGetSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, bool is_subtree) {
  std::vector<GenericSplitter> states (bins_sizes.size());
  for (int i = 0; i < states.size(); i++) {
    auto && state = states[i];
    state.dim = (depth % NDIM);
    state.start_float = universe.lesser_corner[state.dim];
    state.end_float   = universe.greater_corner[state.dim];
    state.goal_rank   = bins_sizes[i] / 2 + (bins_sizes[i] % 2); // left heavy
  }
  int n_pending = states.size();
  while (n_pending > 0) {
    CkReductionMsg *msg;
    readers->countAssignments(states, is_subtree, CkCallbackResumeThread((void*&)msg), false);
    int* counts = (int*)msg->getData();
    for (int i = 0; i < states.size(); i++) {
      auto&& count = counts[i];
      auto&& state = states[i];
      if (!state.pending) continue;
#if DEBUG
      CkPrintf("count %d is %d for goal_rank %d start_range %f end_range %f compare_to %f\n", i, counts[i], state.goal_rank, state.start_float, state.end_float, state.midFloat());
#endif
      bool identical = (state.end_float - state.start_float) <= 2 * std::numeric_limits<Real>::epsilon();
      if (count == state.goal_rank || identical) {
        state.pending = false;
        n_pending--;
      }
      else if (count < state.goal_rank) {
        state.start_float = state.midFloat();
        state.goal_rank -= count;
      }
      else {
        state.end_float = state.midFloat();
      }
    }
  }
  return states;
}

// this is not used by parallelFindSplitters
std::pair<int, Real> KdDecomposition::sortAndGetSplitter(int depth, Bin& bin) {
  static auto compX = [] (const std::pair<int, Vector3D<Real>>& a, const std::pair<int, Vector3D<Real>>& b) {return a.second.x < b.second.x;};
  static auto compY = [] (const std::pair<int, Vector3D<Real>>& a, const std::pair<int, Vector3D<Real>>& b) {return a.second.y < b.second.y;};
  static auto compZ = [] (const std::pair<int, Vector3D<Real>>& a, const std::pair<int, Vector3D<Real>>& b) {return a.second.z < b.second.z;};
  int dim = depth % NDIM;
  size_t medianIndex = (bin.size() + 1) / 2;
  if (dim == 0)      std::nth_element(bin.begin(), bin.begin()+medianIndex, bin.end(), compX);
  else if (dim == 1) std::nth_element(bin.begin(), bin.begin()+medianIndex, bin.end(), compY);
  else if (dim == 2) std::nth_element(bin.begin(), bin.begin()+medianIndex, bin.end(), compZ);
  auto median = bin[medianIndex].second[dim]; // not average?
  return {dim, median};
}

// this is not used by parallelFindSplitters
void LongestDimDecomposition::assign(Bin& parent, Bin& left, Bin& right, BinarySplit split) {
  for (auto && pos : parent) {
    if (pos.second[split.first] > split.second) right.push_back(pos);
    else left.push_back(pos);
  }
}

// this is not used by parallelFindSplitters
std::pair<int, Real> LongestDimDecomposition::sortAndGetSplitter(int depth, Bin& bin) {
  OrientedBox<Real> box;
  Vector3D<Real> unweighted_center;
  for (auto && pos : bin) {
    box.grow(pos.second);
    unweighted_center += pos.second;
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

std::vector<GenericSplitter> LongestDimDecomposition::sortAndGetSplitters(OrientedBox<Real> universe, int n_total_particles, IReader* readers, bool is_subtree) {
  CkReductionMsg *msg;
  std::vector<GenericSplitter> empty;
  readers->countAssignments(empty, is_subtree, CkCallbackResumeThread((void*&)msg), false);
  CkReduction::tupleElement* res = nullptr;
  int numRedn = 0;
  msg->toTuple(&res, &numRedn);
  Vector3D<Real>* centers = (Vector3D<Real>*)(res[0].data);
  int* counts = (int*)(res[1].data);
  Vector3D<Real>* lesser_corners = (Vector3D<Real>*)(res[2].data);
  Vector3D<Real>* greater_corners = (Vector3D<Real>*)(res[3].data);

  std::vector<GenericSplitter> level_splitters;
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
    GenericSplitter sp;
    sp.dim = best_dim;
    sp.start_float = sp.end_float = unweighted_center[best_dim];
    level_splitters.push_back(sp);
  }
  return level_splitters;
}

// called by each reader. NOTE: this is a very different function from KdDecomposition's countAssignments
// it is just trying to get the box dimensions after a binary split
// assumed state: bins has size = splits.size().
// state change: none, unless this is the first time we're doing this. then we use initBinarySplit
// outputs: all the box dimensions. unweighted moment, count, lesser_corner, greater_corner
CkReductionMsg* LongestDimDecomposition::countAssignments(const std::vector<GenericSplitter>& states, const IParticleViewer* particles, bool is_subtree, bool weight_by_partition) {
  if (bins.empty()) initBinarySplit(particles);
  std::vector<Vector3D<Real>> centers (bins.size(), (0,0,0));
  std::vector<int> counts (bins.size(), 0);
  std::vector<Vector3D<Real>> lesser_corner (bins.size());
  std::vector<Vector3D<Real>> greater_corner (bins.size());
  for (int i = 0; i < bins.size(); i++) {
    auto && bin = bins[i];
    OrientedBox<Real> box;
    for (auto && pos : bin) {
      centers[i] += pos.second;
      box.grow(pos.second);
    }
    counts[i] = bin.size();
    lesser_corner[i] = box.lesser_corner;
    greater_corner[i] = box.greater_corner;
  }

  const size_t numTuples = 4;
  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(Vector3D<Real>) * centers.size(), &centers[0], CkReduction::sum_float),
    CkReduction::tupleElement(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int),
    CkReduction::tupleElement(sizeof(Vector3D<Real>) * lesser_corner.size(), &lesser_corner[0], CkReduction::min_float),
    CkReduction::tupleElement(sizeof(Vector3D<Real>) * greater_corner.size(), &greater_corner[0], CkReduction::max_float)
  };
  return CkReductionMsg::buildFromTuple(tupleRedn, numTuples);
}

void LongestDimDecomposition::setArrayOpts(CkArrayOptions& opts, const std::vector<int>& partition_locations, bool collocate) {
  if (collocate) {
    Decomposition::setArrayOpts(opts, partition_locations, collocate);
  }
  else {
    auto myMap = CProxy_DecompArrayMap::ckNew(this, saved_n_total_particles, splitters.size());
    opts.setMap(myMap);
  }
}
