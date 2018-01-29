#include "TipsyFile.h"
#include "Reader.h"
#include "Utility.h"
#include <bitset>
#include <iostream>
#include <cstring>

extern CProxy_Main mainProxy;
extern int n_readers;
extern int decomp_type;

Reader::Reader() {}

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
  cout << "[Reader " << thisIndex << "] Built bounding box: " << box << endl;
#endif

  // reduce to universal bounding box
  contribute(sizeof(BoundingBox), &box, BoundingBox::reducer(), cb);
}

void Reader::assignKeys(BoundingBox& universe, const CkCallback& cb) {
  // generate particle keys
  for (unsigned int i = 0; i < particles.size(); i++) {
    particles[i].key = SFC::generateKey(particles[i].position, universe.box);

    // add placeholder bit
    particles[i].key |= (Key)1 << (KEY_BITS-1);
  }

  // sort particles using their keys
  particles.quickSort();

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

void Reader::pickSamples(const int oversampling_ratio, const CkCallback& cb) {
  Key* sample_keys = new Key[oversampling_ratio];

  // TODO not random, just equal intervals
  for (int i = 0; i < oversampling_ratio; i++) {
    int index = (particles.size() / (oversampling_ratio + 1)) * (i + 1);
    sample_keys[i] = particles[index].key;
  }

  // accumulate samples
  contribute(sizeof(Key) * oversampling_ratio, sample_keys, CkReduction::concat, cb);
  delete[] sample_keys;
}

void Reader::prepMessages(const std::vector<Key>& splitter_keys, const CkCallback& cb) {
  // FIXME locally sort particles and then redistribute?
  // place particles in respective buckets
  std::vector<std::vector<Particle>> send_vectors(n_readers);
  for (int i = 0; i < particles.size(); i++) {
    // TODO
    int bucket = Utility::binarySearchLE(particles[i], &splitter_keys[0], 0, splitter_keys.size());
    send_vectors[bucket].push_back(particles[i]);
  }

  // prepare particle messages
  for (int bucket = 0; bucket < n_readers; bucket++) {
    int n_particles = send_vectors[bucket].size();
    if (n_particles > 0) {
      ParticleMsg* msg = new (n_particles) ParticleMsg(&send_vectors[bucket][0], n_particles);
      particle_messages.push_back(msg);
    }
    else {
      particle_messages.push_back(NULL);
    }
  }

  // zero out local particle vector to receive new particles
  particles.resize(0);

  contribute(cb);
}

void Reader::redistribute() {
  // send particles home
  for (int bucket = 0; bucket < n_readers; bucket++) {
    if (particle_messages[bucket] != NULL)
      thisProxy[bucket].receiveMessage(particle_messages[bucket]);
  }
}

void Reader::receiveMessage(ParticleMsg* msg) {
  static int n_particles = 0;

  // copy particles to local vector
  particles.resize(n_particles + msg->n_particles);
  std::memcpy(&particles[n_particles], msg->particles, msg->n_particles * sizeof(Particle));
  n_particles += msg->n_particles;
  delete msg;
}

void Reader::setSplitters(const CkVec<Splitter>& splitters, const CkCallback& cb) {
  this->splitters = splitters;
  contribute(cb);
}

/*
void Reader::flush(CProxy_TreePiece treepieces) {
  // send particles to owner TreePieces
  int start = 0;
  int finish = particles.size();
  int flush_count = 0;
  Key from, to;

  int max;
  if (decomp_type == OCT_DECOMP) {
    max = splitters.size();
  }
  else if (decomp_type == SFC_DECOMP) {
    // TODO ?
    max = splitters.size();
  }

  for (int i = 0; i < max; i++) {
    if (decomp_type == OCT_DECOMP) {
      from = splitters[i].from;
      to = splitters[i].to;
    }
    else if (decomp_type == SFC_DECOMP) {
      // TODO ?? why not the same?
      from = splitters[i];
      to = splitters[i+1];
    }

    int begin = Utility::binarySearchGE(from, &particles[0], start, finish);
    int end = Utility::binarySearchGE(to, &particles[0], begin, finish);

    int n_particles = end - begin;

    if (n_particles > 0) {
      ParticleMsg *msg = new (n_particles) ParticleMsg(&particles[begin], n_particles);
      treepieces[i].receive(msg);
      flush_count += n_particles;
    }

    start = end;
  }

  if (flush_count != particles.size()) {
    CkPrintf("[Reader %d] flushed %d out of %d particles\n", thisIndex, flush_count, particles.size());
    CkAbort("Flush failure");
  }

  splitters.resize(0);
}
*/
