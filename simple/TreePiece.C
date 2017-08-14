#include "TreePiece.h"
#include "Splitter.h"
#include "Reader.h"

extern CProxy_Reader readers;

TreePiece::TreePiece() {}

void TreePiece::initialize(const CkCallback& cb) {
  const CkVec<Splitter>& splitters = readers.ckLocalBranch()->getSplitters();
  n_expected = splitters[thisIndex].n_particles;
  root = splitters[thisIndex].root;

  contribute(cb);
}

void TreePiece::receive(ParticleMsg* msg) {
  for (int i = 0; i < msg->n_particles; i++) {
    particles.push_back(msg->particles[i]);
  }
  CkAssert(msg->n_particles == particles.size());
  /*
  particles.resize(msg->n_particles);
  memcpy(&particles[particle_index], msg->particles, msg->n_particles * sizeof(Particle));
  particle_index += msg->n_particles;
  */
  delete msg;

  // TODO need to check if all particles are received
}
