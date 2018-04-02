#include "TipsyFile.h"
#include "Reader.h"
#include "Utility.h"
#include <iostream>
#include <cstring>
#include <algorithm>

extern CProxy_Main mainProxy;
extern int n_readers;
extern int decomp_type;

Reader::Reader() : particle_index(0) {}

void Reader::load(std::string input_file, const CkCallback& cb) {
  // open tipsy file
  Tipsy::TipsyReader r(input_file);
  if (!r.status()) {
    CkPrintf("[%u] Could not open tipsy file (%s)\n", thisIndex, input_file.c_str());
    CkExit();
  }

  // read header and count particles
  Tipsy::header tipsyHeader = r.getHeader();
  int n_total = tipsyHeader.nbodies;
  int n_sph = tipsyHeader.nsph;
  int n_dark = tipsyHeader.ndark;
  int n_star = tipsyHeader.nstar;

  int n_particles = n_total / n_readers;
  int excess = n_total % n_readers;
  unsigned int start_particle = n_particles * thisIndex;
  if (thisIndex < (unsigned int)excess) {
    n_particles++;
    start_particle += thisIndex;
  }
  else {
    start_particle += excess;
  }

  // prepare bounding box
  box.reset();
  box.pe = 0.0;
  box.ke = 0.0;

  // reserve space
  particles.resize(n_particles);

  // read particles and grow bounding box
  if (!r.seekParticleNum(start_particle)) {
    CkAbort("Could not seek to particle\n");
  }

  Tipsy::gas_particle gp;
  Tipsy::dark_particle dp;
  Tipsy::star_particle sp;

  for (unsigned int i = 0; i < n_particles; i++) {
    if (start_particle + i < (unsigned int)n_sph) {
      if (!r.getNextGasParticle(gp)) {
        CkAbort("Could not read gas particle\n");
      }
      particles[i].mass = gp.mass;
      particles[i].position = gp.pos;
      particles[i].velocity = gp.vel;
    }
    else if (start_particle + i < (unsigned int)n_sph + (unsigned int)n_dark) {
      if (!r.getNextDarkParticle(dp)) {
        CkAbort("Could not read dark particle\n");
      }
      particles[i].mass = dp.mass;
      particles[i].position = dp.pos;
      particles[i].velocity = dp.vel;
    }
    else {
      if (!r.getNextStarParticle(sp)) {
        CkAbort("Could not read star particle\n");
      }
      particles[i].mass = sp.mass;
      particles[i].position = sp.pos;
      particles[i].velocity = sp.vel;
    }
    particles[i].order = start_particle + i;
    particles[i].potential = 0.0;

    box.grow(particles[i].position);
    box.mass += particles[i].mass;
    box.ke += particles[i].mass * particles[i].velocity.lengthSquared();
    box.pe = 0.0;
  }

  box.ke /= 2.0;
  box.n_particles = particles.size();

#ifdef DEBUG
  std::cout << "[Reader " << thisIndex << "] Built bounding box: " << box << std::endl;
#endif

  // reduce to universal bounding box
  contribute(sizeof(BoundingBox), &box, BoundingBox::reducer(), cb);
}


void Reader::computeUniverseBoundingBox(const CkCallback& cb) {
  box.reset();
  for (std::vector<Particle>::const_iterator it = particles.begin();
       it != particles.end(); ++it) {
    box.grow(it->position);
    box.mass += it->mass;
    box.ke += 0.5 * it->mass * it->velocity.lengthSquared();
    box.n_particles += 1;
  }
  contribute(sizeof(BoundingBox), &box, BoundingBox::reducer(), cb);
}

void Reader::assignKeys(BoundingBox& universe, const CkCallback& cb) {
  // generate particle keys
  for (unsigned int i = 0; i < particles.size(); i++) {
    particles[i].key = SFC::generateKey(particles[i].position, universe.box);

    // add placeholder bit
    particles[i].key |= (Key)1 << (KEY_BITS-1);
  }

  if (decomp_type == OCT_DECOMP) {
    // sort particles for decomposition
    // no need for SFC, as particles will be sorted globally
    std::sort(particles.begin(), particles.end());
  }

  // back to callee
  contribute(cb);
}
void Reader::countOct(std::vector<Key>& splitter_keys, const CkCallback& cb) {
  std::vector<int> counts;
  counts.resize(splitter_keys.size()/2);
  for (int i = 0; i < splitter_keys.size(); i++) {
    splitter_keys[i] = Utility::removeLeadingZeros(splitter_keys[i]);
  }

  // search for the first particle whose key is greater or equal to the input key,
  // in the range [start, finish)
  // should also work for OCT as the particle keys are SFC keys
  int start = 0;
  int finish = particles.size();
  Key from, to;
  if (particles.size() > 0) {
    for (int i = 0; i < counts.size(); i++) {
      from = splitter_keys[2*i];
      to = splitter_keys[2*i+1];

      int begin = Utility::binarySearchGE(from, &particles[0], start, finish);
      int end = Utility::binarySearchGE(to, &particles[0], begin, finish);
      counts[i] = end - begin;

      start = end;
    }
  }
  else { // no particles
    for (int i = 0; i < counts.size(); i++){
      counts[i] = 0;
    }
  }

  contribute(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int, cb);
}

/*
void Reader::countSfc(const std::vector<Key>& splitter_keys, const CkCallback& cb) {
  std::vector<int> counts;
  counts.resize(splitters.size()-1); // size equal to number of TreePieces

  // TODO code duplication with countOct()
  // search for the first particle whose key is greater or equal to the input key,
  // in the range [start, finish)
  int start = 0;
  int finish = particles.size();
  Key from, to;
  if (particles.size() > 0) {
    for (int i = 0; i < counts.size(); i++) {
      from = splitter_keys[i];
      to = splitter_keys[i+1];

      int begin = Utility::binarySearchGE(from, &particles[0], start, finish); // hmm how does this work for OCT
      int end = Utility::binarySearchGE(to, &particles[0], begin, finish);
      counts[i] = end - begin;

      start = end;
    }
  }
  else { // no particles
    for (int i = 0; i < counts.size(); i++){
      counts[i] = 0;
    }
  }

    contribute(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int, cb);
}
*/

void Reader::pickSamples(const int oversampling_ratio, const CkCallback& cb) {
  Key* sample_keys = new Key[oversampling_ratio];

  // not random, just equal intervals
  for (int i = 0; i < oversampling_ratio; i++) {
    int index = (particles.size() / (oversampling_ratio + 1)) * (i + 1);
    sample_keys[i] = particles[index].key;
  }

  // accumulate samples
  contribute(sizeof(Key) * oversampling_ratio, sample_keys, CkReduction::concat, cb);
  delete[] sample_keys;
}

void Reader::prepMessages(const std::vector<Key>& splitter_keys, const CkCallback& cb) {
  // place particles in respective buckets
  std::vector<std::vector<Particle>> send_vectors(n_readers);
  for (int i = 0; i < particles.size(); i++) {
    // use upper bound splitter index to determine Reader index
    // [lower splitter, upper splitter)
    int bucket = Utility::binarySearchG(particles[i], &splitter_keys[0], 0, splitter_keys.size()) - 1;
    send_vectors[bucket].push_back(particles[i]);
  }

  // prepare particle messages
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

  // check if all particles are assigned to buckets
  if (new_total != old_total)
    CkAbort("Failed to move all particles into buckets");

  contribute(cb);
}

void Reader::redistribute() {
  // send particles home
  for (int bucket = 0; bucket < n_readers; bucket++) {
    if (particle_messages[bucket] != NULL) {
#ifdef DEBUG
      CkPrintf("[Reader %d] Sending %d particles to Reader %d\n", thisIndex,
          particle_messages[bucket]->n_particles, bucket);
#endif
      thisProxy[bucket].receive(particle_messages[bucket]);
    }
  }
}

void Reader::receive(ParticleMsg* msg) {
  // copy particles to local vector
  particles.resize(particle_index + msg->n_particles);
  std::memcpy(&particles[particle_index], msg->particles, msg->n_particles * sizeof(Particle));
  particle_index += msg->n_particles;
  delete msg;
  SFCsplitters.push_back(Key(0)); // maybe use something different than splitters variable?
}

void Reader::localSort(const CkCallback& cb) {
  std::sort(particles.begin(), particles.end());

  contribute(cb);
}

void Reader::checkSort(const Key last, const CkCallback& cb) {
  bool sorted = true;

  if (particles.size() > 0) {
    if (last > particles[0]) {
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

void Reader::setSplitters(const std::vector<Splitter>& splitters, const CkCallback& cb) {
  this->splitters = splitters;
  contribute(cb);
}
