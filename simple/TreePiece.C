#include <fstream>
#include <bitset>
#include "TreePiece.h"
#include "Reader.h"

extern CProxy_Reader readers;
extern int n_chares;
extern int max_ppl;

TreePiece::TreePiece() {}

/*
void TreePiece::initialize(const CkCallback& cb) {
  n_expected = readers.ckLocalBranch()->splitter_counts[thisIndex];
  tp_key = readers.ckLocalBranch()->splitters[thisIndex];
  tp_key |= (Key)1 << (KEY_BITS-1); // add placeholder bit
  cur_idx = 0;

  contribute(cb);
}
*/

void TreePiece::create() {
  n_expected = readers.ckLocalBranch()->splitters[thisIndex].n_particles;
  tp_key = readers.ckLocalBranch()->splitters[thisIndex].tp_key;
  cur_idx = 0;
}

void TreePiece::receive(ParticleMsg* msg) {
  // copy particles to local vector
  particles.resize(particles.size() + msg->n_particles);
  memcpy(&particles[cur_idx], msg->particles, msg->n_particles * sizeof(Particle));

  cur_idx += msg->n_particles;

  delete msg;
}

void TreePiece::check(const CkCallback& cb) {
  if (n_expected != particles.size()) {
    CkPrintf("[TP %d] ERROR! Not enough particles received\n", thisIndex);
    CkExit();
  }

  contribute(cb);
}

void TreePiece::build(const CkCallback &cb){
  // store first and last keys
  first_key = particles[0].key;
  last_key = particles[particles.size()-1].key;
#if DEBUG
  std::cout << "[TP " << thisIndex << "] " << std::bitset<64>(tp_key) << ", " << std::bitset<64>(first_key) << " -> " << std::bitset<64>(last_key) << std::endl;
#endif

  // create global root and recurse
  root = new Node(1, SFC::firstPossibleKey, SFC::lastPossibleKey, 0, particles.size(), NULL);
  recursiveBuild(root, false);

#if DEBUG
  //CkPrintf("[TP %d] Build done\n", thisIndex);
#endif
  contribute(cb);
}

void TreePiece::recursiveBuild(Node* node, bool saw_tp_key) {
  // TODO BIG PROBLEM: splitter key in our case is different from tpRootKey in Distree, where it is a Oct decomposition node key
  // Distree is using Binary decomposition based on SFC keys, but we are using SFC decomposition on SFC keys

  // check if we have seen splitter key
  if (!saw_tp_key) {
    saw_tp_key = (node->key == tp_key);
  }

  // check if node has less particles than threshold
  bool is_light = (node->n_particles <= BUCKET_TOLERANCE * max_ppl);

  // check if the node's key is a prefix to the TreePiece's root
  bool is_prefix = Utility::isPrefix(node->key, Utility::getDepthFromKey(node->key), tp_key, Utility::getDepthFromKey(tp_key));

  if (saw_tp_key && is_light) {
    // we are under the splitter key in the tree and the node is light,
    // so we can make the node a local leaf
    if (node->n_particles == 0)
      node->type = Node::EmptyLeaf;
    else
      node->type = Node::Leaf;
  }
  else if (is_light && !is_prefix) {
    // the node is light but we haven't seen the splitter key yet, 
  }
}
