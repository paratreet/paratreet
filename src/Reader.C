#include "TipsyFile.h"
#include "Reader.h"
#include "Utility.h"
#include "Modularization.h"
#include <iostream>
#include <cstring>
#include <algorithm>

Reader::Reader() : particle_index(0) {}

void Reader::load(std::string input_file, const CkCallback& cb) {
  // Open tipsy file
  Tipsy::TipsyReader r(input_file);
  if (!r.status()) {
    CkPrintf("Reader %d failed to open tipsy file %s\n", thisIndex, input_file.c_str());
    CkAbort("Tipsy reading failure in Reader -- see stdout");
  }

  // Read header and count particles
  Tipsy::header tipsyHeader = r.getHeader();
  int n_total = tipsyHeader.nbodies;
  int n_sph = tipsyHeader.nsph;
  int n_dark = tipsyHeader.ndark;
  int n_star = tipsyHeader.nstar;
  start_time = tipsyHeader.time;

  int n_particles = n_total / n_readers;
  int excess = n_total % n_readers;
  unsigned int start_particle = n_particles * thisIndex;
  if (thisIndex < (unsigned int)excess) {
    n_particles++;
    start_particle += thisIndex;
  } else {
    start_particle += excess;
  }

  // Prepare bounding box
  BoundingBox box;
  box.pe = 0.0;
  box.ke = 0.0;

  // Reserve space
  particles.resize(n_particles);

  // Read particles and grow bounding box
  if (!r.seekParticleNum(start_particle)) {
    CkAbort("Could not seek to particle\n");
  }

  Tipsy::gas_particle gp;
  Tipsy::dark_particle dp;
  Tipsy::star_particle sp;

  for (unsigned int i = 0; i < n_particles; i++) {
    particles[i].potential = particles[i].u = particles[i].soft = 0u;
    if (start_particle + i < (unsigned int)n_sph) {
      if (!r.getNextGasParticle(gp)) {
        CkAbort("Could not read gas particle\n");
      }
      particles[i].mass = gp.mass;
      particles[i].soft = gp.hsmooth;
      particles[i].position = gp.pos;
      particles[i].velocity = gp.vel;
      particles[i].u = gp.temp * gasConstant / gammam1 / meanMolWeight;
      particles[i].type = Particle::Type::eGas;
      box.n_sph++;
    }
    else if (start_particle + i < (unsigned int)n_sph + (unsigned int)n_dark) {
      if (!r.getNextDarkParticle(dp)) {
        CkAbort("Could not read dark particle\n");
      }
      particles[i].mass = dp.mass;
      particles[i].position = dp.pos;
      particles[i].velocity = dp.vel;
      particles[i].soft = dp.eps;
      particles[i].type = Particle::Type::eDark;
      box.n_dark++;
    }
    else {
      if (!r.getNextStarParticle(sp)) {
        CkAbort("Could not read star particle\n");
      }
      particles[i].mass = sp.mass;
      particles[i].position = sp.pos;
      particles[i].velocity = sp.vel;
      particles[i].type = Particle::Type::eStar;
      box.n_star++;
    }
    particles[i].order = start_particle + i;
    particles[i].velocity_predicted = particles[i].velocity;
    particles[i].u_predicted = particles[i].u;
    box.grow(particles[i].position);
    box.mass += particles[i].mass;
    box.ke += particles[i].mass * particles[i].velocity.lengthSquared();
    box.pe = 0.0;
    particles[i].finishInit();
  }

  box.ke /= 2.0;
  box.n_particles = particles.size();

#if DEBUG
  std::cout << "[Reader " << thisIndex << "] Built bounding box: " << box << std::endl;
#endif

  // Reduce to universal bounding box
  contribute(sizeof(BoundingBox), &box, BoundingBox::reducer(), cb);
}


void Reader::computeUniverseBoundingBox(const CkCallback& cb) {
  BoundingBox box;
  for (std::vector<Particle>::const_iterator it = particles.begin();
       it != particles.end(); ++it) {
    box.grow(it->position);
    box.mass += it->mass;
    box.ke += 0.5 * it->mass * it->velocity.lengthSquared();
    box.n_particles += 1;
  }
  contribute(sizeof(BoundingBox), &box, BoundingBox::reducer(), cb);
}

void Reader::assignKeys(BoundingBox universe_, const CkCallback& cb) {
  // Generate particle keys
  universe = universe_;

  treespec.ckLocalBranch()->getPartitionDecomposition()->assignKeys(universe, particles);

  // Back to callee
  contribute(cb);
}

void Reader::countSfc(const std::vector<QuickSelectSFCState>& states, size_t log_branch_factor, const CkCallback& cb) {
  std::vector<int> counts (states.size(), 0);
  std::function<bool(const Particle&, Key)> compGE = [] (const Particle& a, Key b) {return a.key >= b;};
  if (particles.size() > 0) {
    for (size_t i = 0u; i < states.size(); i++) {
      if (!states[i].pending) continue;
      int begin = Utility::binarySearchComp(states[i].start_range, &particles[0], 0, particles.size(), compGE);
      int found = Utility::binarySearchComp(states[i].compare_to(), &particles[0], begin, particles.size(), compGE);
      counts[i] = found - begin;
    }
  }
  contribute(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int, cb);
}

void Reader::initBinarySplit(const CkCallback& cb) {
  bins.clear();
  bins.emplace_back();
  for (auto && particle : particles) bins.back().push_back(particle.position);
  contribute(cb);
}

void Reader::countKd(const std::vector<QuickSelectKDState>& states, const CkCallback& cb) {
  std::vector<int> counts (states.size(), 0);
  for (int i = 0; i < states.size(); i++) {
    auto && state = states[i];
    if (!state.pending) continue;
    auto && bin = bins[i];
    for (auto && pos : bin) {
      if (pos[state.dim] > state.start_range &&
          pos[state.dim] < state.compare_to()) counts[i]++;
    }
  }
  contribute(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int, cb);
}

void Reader::countLongestDim(const CkCallback& cb) {
  std::vector<Vector3D<Real>> centers (bins.size(), (0,0,0));
  std::vector<int> counts (bins.size(), 0);
  std::vector<Vector3D<Real>> lesser_corner (bins.size());
  std::vector<Vector3D<Real>> greater_corner (bins.size());
  for (int i = 0; i < bins.size(); i++) {
    auto && bin = bins[i];
    OrientedBox<Real> box;
    for (auto && pos : bin) {
      centers[i] += pos;
      box.grow(pos);
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
  CkReductionMsg * msg = CkReductionMsg::buildFromTuple(tupleRedn, numTuples);
  msg->setCallback(cb);
  this->contribute(msg);
}

void Reader::doBinarySplit(const std::vector<std::pair<int, Real>>& splits, const CkCallback& cb) {
  CkAssert(bins.size() == splits.size());
  decltype(bins) binsCopy (2 * bins.size());
  for (int i = 0; i < bins.size(); i++) {
    std::vector<Vector3D<Real>> left, right;
    for (auto && pos : bins[i]) {
      if (pos[splits[i].first] > splits[i].second) {
        binsCopy[2 * i + 1].push_back(pos);
      }
      else {
        binsCopy[2 * i].push_back(pos); // left heavy
      }
    }
  }
  bins = binsCopy;
  std::vector<int> counts (bins.size(), 0);
  for (int i = 0; i < counts.size(); i++) counts[i] = bins[i].size();
  contribute(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int, cb);
}

void Reader::countOct(std::vector<Key> splitter_keys, size_t log_branch_factor, const CkCallback& cb) {
  std::vector<int> counts (splitter_keys.size()/2, 0);
  for (int i = 0; i < splitter_keys.size(); i++) {
    splitter_keys[i] = Utility::removeLeadingZeros(splitter_keys[i], log_branch_factor);
  }

  // Search for the first particle whose key is greater or equal to the input key,
  // in the range [start, finish). This should also work for OCT as the particle
  // keys are SFC keys.
  int start = 0;
  int finish = particles.size();
  Key from, to;
  std::function<bool(const Particle&, Key)> compGE = [] (const Particle& a, Key b) {return a.key >= b;};
  if (particles.size() > 0) {
    for (int i = 0; i < counts.size(); i++) {
      from = splitter_keys[2*i];
      to = splitter_keys[2*i+1];

      int begin = Utility::binarySearchComp(from, &particles[0], start, finish, compGE);
      int end = Utility::binarySearchComp(to, &particles[0], begin, finish, compGE);
      counts[i] = end - begin;

      start = end;
    }
  }

  contribute(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int, cb);
}

void Reader::getAllSfcKeys(const CkCallback& cb)
{
  std::vector<Key> keys;
  for (const auto& p : particles)
    keys.push_back(p.key);

  contribute(keys.size() * sizeof(Key), &keys[0], CkReduction::set, cb);
}

void Reader::getAllPositions(const CkCallback& cb)
{
  std::vector<Vector3D<Real>> positions;
  for (const auto& p : particles)
    positions.push_back(p.position);

  contribute(positions.size() * sizeof(Vector3D<Real>), &positions[0], CkReduction::set, cb);
}

void Reader::pickSamples(const int oversampling_ratio, const CkCallback& cb) {
  Key* sample_keys = new Key[oversampling_ratio];

  // Not random, just equal intervals
  for (int i = 0; i < oversampling_ratio; i++) {
    int index = (particles.size() / (oversampling_ratio + 1)) * (i + 1);
    sample_keys[i] = particles[index].key;
  }

  // Accumulate samples
  contribute(sizeof(Key) * oversampling_ratio, sample_keys, CkReduction::concat, cb);
  delete[] sample_keys;
}

void Reader::prepMessages(const std::vector<Key>& splitter_keys, const CkCallback& cb) {
  // Place particles in respective buckets
  std::vector<std::vector<Particle>> send_vectors(n_readers);
  for (int i = 0; i < particles.size(); i++) {
    // Use upper bound splitter index to determine Reader index
    // [lower splitter, upper splitter)
    int bucket = Utility::binarySearchG(particles[i].key, &splitter_keys[0], 0, splitter_keys.size()) - 1;
    send_vectors[bucket].push_back(particles[i]);
  }

  // Prepare particle messages
  int old_total = particles.size();
  int new_total = 0;
  for (int bucket = 0; bucket < n_readers; bucket++) {
    int n_particles = send_vectors[bucket].size();
    new_total += n_particles;

    if (n_particles > 0) {
      ParticleMsg* msg = new (n_particles) ParticleMsg(&send_vectors[bucket][0], n_particles);
      particle_messages.push_back(msg);
    }
    else {
      particle_messages.push_back(NULL);
    }
  }

  // Check if all particles are assigned to buckets
  if (new_total != old_total)
    CkAbort("Failed to move all particles into buckets");

  contribute(cb);
}

void Reader::redistribute() {
  // Send particles home
  for (int bucket = 0; bucket < n_readers; bucket++) {
    if (particle_messages[bucket] != NULL) {
#if DEBUG
      CkPrintf("[Reader %d] Sending %d particles to Reader %d\n", thisIndex,
          particle_messages[bucket]->n_particles, bucket);
#endif
      thisProxy[bucket].receive(particle_messages[bucket]);
    }
  }
}

void Reader::receive(ParticleMsg* msg) {
  // Copy particles to local vector
  particles.resize(particle_index + msg->n_particles);
  std::memcpy(&particles[particle_index], msg->particles, msg->n_particles * sizeof(Particle));
  particle_index += msg->n_particles;
  delete msg;
  // SFCsplitters.push_back(Key(0)); // Maybe use something different than splitters variable?
}

void Reader::localSort(const CkCallback& cb) {
  std::sort(particles.begin(), particles.end());

  contribute(cb);
}

void Reader::checkSort(const Key last, const CkCallback& cb) {
  bool sorted = true;

  if (particles.size() > 0) {
    if (last > particles[0].key) {
      sorted = false;
    }
    else {
      for (int i = 0; i < particles.size()-1; i++) {
        if (particles[i] > particles[i+1]) {
          sorted = false;
          break;
        }
      }
    }
  }

  if (sorted)
    CkPrintf("[Reader %d] Particles sorted\n", thisIndex);
  else
    CkPrintf("[Reader %d] Particles NOT sorted\n", thisIndex);

  if (thisIndex < n_readers-1) {
    Key my_last;
    if (particles.size() > 0)
      my_last = particles.back().key;
    else
      my_last = Key(0);

    thisProxy[thisIndex+1].checkSort(my_last, cb);
    contribute(cb);
  }
  else {
    contribute(cb);
  }
}
