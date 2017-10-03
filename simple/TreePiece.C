#include <fstream>
#include "TreePiece.h"
#include "Reader.h"

extern CProxy_Reader readers;
extern int n_chares;
extern int max_ppl;

TreePiece::TreePiece() {}

void TreePiece::initialize(const CkCallback& cb) {
  if (thisIndex < n_chares) {
    n_expected = readers.ckLocalBranch()->splitter_counts[thisIndex];
    first_key = readers.ckLocalBranch()->splitters[thisIndex];
  }
  else {
    n_expected = 0;
    first_key = 0;
  }
  cur_idx = 0;

  contribute(cb);
}

void TreePiece::receive(ParticleMsg* msg) {
  particles.resize(particles.size() + msg->n_particles);
  memcpy(&particles[cur_idx], msg->particles, msg->n_particles * sizeof(Particle));
  cur_idx += msg->n_particles;
  /*
  for (int i = 0; i < msg->n_particles; i++) {
    particles.push_back(msg->particles[i]);
  }
  */
  delete msg;
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

void TreePiece::build(const CkCallback &cb){
  root = new Node(SFC::firstPossibleKey, SFC::lastPossibleKey, 0, particles.size(), NULL);

  /*
  if(thisIndex < n_chares){
    Particle* local_particles = NULL;
    if(particles.size() > 0){
      local_particles = &particles[0];
    }

    // arguments to constructor of Node are:
    // key, depth, particle start index, num particles
    root_node = new Node(Key(1), 0, local_particles, particles.size());
    // global root is shared by all (useful) tree pieces
    root_node->setOwnerStart(0);
    root_node->setOwnerEnd(n_chares);

    root_node->setParent(NULL);

    // do only local build; don't ask for remote nodes'
    // payloads, but ensure they are correctly labeled;
    // submit tree to manager when done
    build(root_node, false);

    //treeHandle.registration(root_node, this);
  }
  else{
    root_node = NULL;
    CkAssert(particles.size() == 0);
  }
  */

#if DEBUG
  CkPrintf("[TP %d] Build done\n", thisIndex);
#endif
  contribute(cb);
}

void TreePiece::recursiveBuild(Node* node) {
  // check if number of particles is below threshold
  if (node->n_particles > max_ppl) {
    // create children and check if bounding box has been divided
  }
}
