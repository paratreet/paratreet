#include "TipsyFile.h"
#include "Reader.h"
#include "Utility.h"

extern CProxy_Main mainProxy;
extern int n_readers;

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
  Vector3D<Real> universe_diag = universe.box.greater_corner - universe.box.lesser_corner;
  Vector3D<Real> relative_pos;

  // prepare mask to set MSB to 1
  Key prepend = 1L << (KEY_BITS-1);

  // FIXME could be heavy work for a single PE to handle
  for (unsigned int i = 0; i < particles.size(); i++) {
    // compute key
    relative_pos = (particles[i].position - universe.box.lesser_corner) / universe_diag * ((Real)BOXES_PER_DIM);

    Key xint = (Key)relative_pos.x;
    Key yint = (Key)relative_pos.y;
    Key zint = (Key)relative_pos.z;

    // interleave bits
    Key mask = Key(0x1);
    Key k = Key(0x0);
    int shift_by = 0;
    for (int j = 0; j < BITS_PER_DIM; j++) {
      k |= (zint & mask) << shift_by;
      k |= (yint & mask) << (shift_by+1);
      k |= (xint & mask) << (shift_by+2);
      mask <<= 1;
      shift_by += (NDIM-1);
    }

    // set MSB to 1
    k |= prepend;

    // save key
    particles[i].key = k;
  }

  // sort particles using their keys
  particles.quickSort();

  // back to callee
  contribute(cb);
}

void Reader::count(CkVec<Key>& keys, const CkCallback& cb) {
  // convert keys to splitters
  for (int i = 0; i < keys.size(); i++) {
    // ignore largest key as it is a splitter
    if (keys[i] == (~Key(0)))
        continue;

    Key& k = keys[i];
    k = Splitter::convertKey(k);
  }

  CkVec<int> counts(keys.size() / 2);
  CkAssert(counts.size() * 2 == keys.size());

  // search for the first particle whose key is greater or equal to the input key,
  // in the range [start, finish)
  int start = 0;
  int finish = particles.size();
  if (particles.size() > 0) {
    for (int i = 0; i < counts.size(); i++) {
      Key from = keys[2*i];
      Key to = keys[2*i+1];

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

void Reader::setSplitters(CkVec<Splitter>& sp, const CkCallback& cb) {
  splitters = sp;
  contribute(cb);
}
