#include "TreePiece.h"
#include "Splitter.h"
#include "Reader.h"

extern CProxy_Reader readers;

TreePiece::TreePiece() {}

void TreePiece::create(const CkCallback& cb) {
  const CkVec<Splitter>& splitters = readers.ckLocalBranch()->getSplitters();
  if (thisIndex < splitters.size()) {
    n_expected = splitters[thisIndex].n_particles;
    root = splitters[thisIndex].root;
  }
  else {
    n_expected = 0;
    root = 0;
  }

  contribute(cb);
}

void TreePiece::receive(ParticleMsg* msg) {
  for (int i = 0; i < msg->n_particles; i++) {
    particles.push_back(msg->particles[i]);
  }
  delete msg;
/*
  particles.resize(n_particles + msg->n_particles);
  memcpy(&particles[particle_index], msg->particles, msg->n_particles * sizeof(Particle));
  particle_index += msg->n_particles;
*/
}

void TreePiece::check(const CkCallback& cb) {
  if (n_expected != particles.size()) {
    CkPrintf("[TP %d] ERROR! Not enough particles received\n", thisIndex);
    CkExit();
  }
#if DEBUG
  CkPrintf("[TP %d] Has %d particles\n", thisIndex, particles.size());
#endif

  contribute(cb);
}
