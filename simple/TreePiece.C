#include "TreePiece.h"
#include "Splitter.h"
#include "Reader.h"

extern CProxy_Reader readers;

TreePiece::TreePiece() {
  const CkVec<Splitter>& splitters = readers.ckLocalBranch()->getSplitters();
  n_expected = splitters[thisIndex].n_particles;
  root = splitters[thisIndex].root;
}

void TreePiece::receive(int n, Particle* p) {
  for (int i = 0; i < n; i++) {
    particles.push_back(p[i]);
  }

  // TODO need to check if all particles are received
}
