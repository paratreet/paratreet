#include "TreePiece.h"
#include "Splitter.h"
#include "Reader.h"

extern CProxy_Reader readers;

TreePiece::TreePiece() {}

void TreePiece::create(const CkCallback& cb) {
  const CkVec<Splitter>& splitters = readers.ckLocalBranch()->getSplitters();
  nPieces = splitters.size();
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

void TreePiece::build(const CkCallback &cb){
  callback_ = cb;

  if(thisIndex < nPieces){
    Particle *localParticles = NULL;
    if(particles.size() > 0){
      localParticles = &particles[0];
    }

    // arguments to constructor of Node are:
    // key, depth, particle start index, num particles 
    root_ = new Node(Key(1), 0, localParticles, particles.size());
    // global root is shared by all (useful) tree pieces
    root_->setOwnerStart(0);
    root_->setOwnerEnd(nPieces);

    root_->setParent(NULL);

    const CkVec<Splitter> &splitters = readers.ckLocalBranch()->getSplitters();

    // do only local build; don't ask for remote nodes'
    // payloads, but ensure they are correctly labeled;
    // submit tree to manager when done

    //build(splitters, root_, false); //TODO

    //print(root_);
    //treeHandle.registration(root_, this);
  }
  else{
    root_ = NULL;
    CkAssert(particles.size() == 0);
  }

  CkPrintf("[TP %d] Build done\n", thisIndex);
  contribute(cb);
}
