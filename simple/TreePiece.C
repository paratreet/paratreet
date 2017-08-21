#include "TreePiece.h"
#include "Splitter.h"
#include "Reader.h"

extern CProxy_Reader readers;
extern int max_ppl;

TreePiece::TreePiece() {}

void TreePiece::create(const CkCallback& cb) {
  const CkVec<Splitter>& splitters = readers.ckLocalBranch()->getSplitters();
  nPieces = splitters.size();
  if (thisIndex < nPieces) {
    n_expected = splitters[thisIndex].n_particles;
    root = splitters[thisIndex].root;
    tpRootKey_ = splitters[thisIndex].root;
  }
  else {
    n_expected = 0;
    root = 0;
    tpRootKey_ = Key(0);
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

    build(splitters, root_, false);

    //treeHandle.registration(root_, this);
  }
  else{
    root_ = NULL;
    CkAssert(particles.size() == 0);
  }

#if DEBUG
  CkPrintf("[TP %d] Build done\n", thisIndex);
  if (thisIndex == 0) print(root_);
#endif
  contribute(cb);
}

void TreePiece::print(Node *root){
  ostringstream oss;
  oss << "tree." << thisIndex << ".dot";
  std::ofstream out(oss.str().c_str());
  CkAssert(out.is_open());
  out << "digraph tree" << thisIndex << "{" << endl;
  root->dot(out);
  out << "}" << endl;
  out.close();
}


// returns whether node and everything below it is entirely local
bool TreePiece::build(const CkVec<Splitter> &splitters, Node *node, bool rootLiesOnPath){
  node->tp() = thisIndex;
  // if we haven't seen the root of the tp yet, check whether
  // current node is the root; otherwise, don't perform this check:
  // once the tp root has been seen along a path, it stays that way
  if(!rootLiesOnPath){
    rootLiesOnPath = (node->getKey() == tpRootKey_);
  }

  int nodeNumParticles = node->getNumParticles();
  bool isLight = (nodeNumParticles <= BUCKET_TOLERANCE * Real(max_ppl));
  if(rootLiesOnPath && isLight){
    // we have seen the tp root along the path to this node, and
    // this node is light enough to be made a leaf
    // therefore, local leaf
    if(nodeNumParticles == 0) node->setType(EmptyLeaf);
    else node->setType(Leaf);

    return true;
  }
  else if(isLight && !Utility::isPrefix(node->getKey(), node->getDepth(),
              tpRootKey_, Utility::getDepthFromKey(tpRootKey_))){
    // if we haven't seen the tp root yet, but encounter a light node,
    // this node could still be on the path from the global root to the
    // tp root; therefore, we do the Utility::isPrefix check. if in 
    // addition to the previous conditions, the node is off-path, then
    // it is a remote node.
    CkAssert(nodeNumParticles == 0);
    // we can distinguish between
    // remote nodes and remote leaves at this point,
    // since we have the number of particles 
    // assigned to all the tree pieces, and in 
    // particular, the number of particles assigned
    // to the tree piece with this node as
    // its root
    int ownerStart = node->getOwnerStart();
    int ownerEnd = node->getOwnerEnd();
    int askWhom = -1;

    CkAssert(node->getNumChildren() == 0);
    CkAssert(node->getChildren() == NULL);

    if(ownerStart + 1 == ownerEnd){
      // single owner, i.e. is a tree piece root
      // how many particles were assigned to this tree piece?
      int nParticlesAssigned = splitters[ownerStart].n_particles;
      if(nParticlesAssigned == 0){
        node->setType(RemoteEmptyLeaf);
      }
      else if(nParticlesAssigned <= BUCKET_TOLERANCE * Real(max_ppl)){
        node->setType(RemoteLeaf);
      }
      else{
        node->setType(Remote);
        // we know that this node is not a leaf, so 
        // it must have children in the tree piece that
        // owns it.
        node->setNumChildren(BRANCH_FACTOR);
      }
    }
    else{
      node->setType(Remote);
      // again, this node is not a leaf, so it must
      // have children in its owner
      node->setNumChildren(BRANCH_FACTOR);
    }


    return false;
  }

  // From Distree barnes/Main.cpp:88-89
  // nNodeChildren and nNodeChildrenLg are
  // BRANCH_FACTOR and LOG_BRANCH_FACTOR

  int nNodeChildren = BRANCH_FACTOR;
  int nNodeChildrenLg = LOG_BRANCH_FACTOR;

  Key childKey = (node->getKey() << nNodeChildrenLg); 

  Particle *nodeParticles = node->getParticles();

  int start = 0;
  int finish = start + node->getNumParticles();

  int ownerStart = node->getOwnerStart();
  int ownerEnd = node->getOwnerEnd();
  bool singleOwnerTp = (ownerStart + 1 == ownerEnd);

  node->allocateChildren(nNodeChildren); 

  int nonLocalChildren = 0;
  for(int i = 0; i < nNodeChildren; i++){
    int firstGeIdx;

    Key siblingSplitterKey = Splitter::convertKey(childKey + 1);
    if(i < nNodeChildren-1){
      firstGeIdx = Utility::binarySearchGE(siblingSplitterKey, nodeParticles, start, finish);
    }
    else{
      firstGeIdx = finish; 
    }
    int np = firstGeIdx - start;

    Node *child;
    child = new (node->getChild(i)) Node(childKey, node->getDepth() + 1, nodeParticles + start, np);
    child->setParent(node);
    
    // set owner tree pieces of child
    child->setOwnerStart(ownerStart);
    if(singleOwnerTp){
      child->setOwnerEnd(ownerEnd);
    }
    else{
      if(i < nNodeChildren-1){
        // if you have a right sibling, search through array of assigned
        // splitters and find the  index of the splitter that has the first key
        // GEQ the right sibling's, i.e. the first key not underneath this child
        int firstGeTp = Utility::binarySearchGE(siblingSplitterKey, &splitters[0], ownerStart, ownerEnd); 
        child->setOwnerEnd(firstGeTp);
        ownerStart = firstGeTp;
      }
      else{
        // for last child, there is no right sibling, and its
        // owner end is same as parent's
        child->setOwnerEnd(node->getOwnerEnd());
      }
    }

    bool local = build(splitters, child, rootLiesOnPath);

    if (!local) {
      nonLocalChildren++;
    }

    start = firstGeIdx;
    childKey++;
  }

  if(nonLocalChildren == 0){
    node->setType(Internal);
  }
  else{
    node->setType(Boundary);
  }

  return (nonLocalChildren == 0);
}
