#include "TipsyFile.h"
#include "Reader.h"
#include "Utility.h"
#include <bitset>
#include <iostream>

extern CProxy_Main mainProxy;
extern CProxy_TreePiece treepieces;
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

void Reader::count(CkVec<Key>& splitters, const CkCallback& cb) {
  std::vector<int> counts;
  if (decomp_type == OCT_DECOMP) {
    counts.resize(splitters.size() / 2);
    for (int i = 0; i < splitters.size(); i++) {
        splitters[i] = Utility::removeLeadingZeros(splitters[i]);
    }
  }
  else if (decomp_type == SFC_DECOMP) {
    counts.resize(splitters.size()-1);
  }

  // search for the first particle whose key is greater or equal to the input key,
  // in the range [start, finish)
  int start = 0;
  int finish = particles.size();
  Key from, to;
  if (particles.size() > 0) {
    for (int i = 0; i < counts.size(); i++) {
      if (decomp_type == OCT_DECOMP) {
        from = splitters[2*i];
        to = splitters[2*i+1];
      }
      else if (decomp_type == SFC_DECOMP) {
        from = splitters[i];
        to = splitters[i+1];
      }

      int begin = Utility::binarySearchGE(from, &particles[0], start, finish);
      int end = Utility::binarySearchGE(to, &particles[0], begin, finish);
      counts[i] = end - begin;

      start = end;
    }
  }
  else { // no particles
    for(int i = 0; i < counts.size(); i++){
      counts[i] = 0;
    }
  }

  contribute(sizeof(int) * counts.size(), &counts[0], CkReduction::sum_int, cb);
}

void Reader::setSplitters(CkVec<Splitter>& splitters, const CkCallback& cb) {
  this->splitters = splitters;
  contribute(cb);
}

/*
void Reader::setSplitters(const std::vector<Key>& sp, const CkCallback& cb) {
  splitters = sp;
  contribute(cb);
}

void Reader::setSplitters(const std::vector<Key>& sp, const std::vector<int>& bin_counts, const CkCallback& cb) {
  splitters = sp;
  splitter_counts = bin_counts;
  contribute(cb);
}
*/

void Reader::flush() {
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
    // TODO
  }

  for (int i = 0; i < max; i++) {
    if (decomp_type == OCT_DECOMP) {
      from = splitters[i].from;
      to = splitters[i].to;
    }
    else if (decomp_type == SFC_DECOMP) {
      // TODO
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
