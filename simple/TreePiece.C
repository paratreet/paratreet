#include <fstream>
#include <sstream>
#include <bitset>
#include "TreePiece.h"
#include "Reader.h"
#include "TreeElements.h"

extern CProxy_Reader readers;
extern int max_particles_per_leaf;
extern int tree_type;

void TreePiece::calculateData(CProxy_TreeElements tree_array) {
  Vector3D<Real> moment;
  Real summass = 0;
  for (int i = 0; i < particles.size(); i++) {
    moment += particles[i].position * particles[i].mass;
    summass += particles[i].mass;
  }
  tree_array[tp_key].receiveData(moment, summass);
}


TreePiece::TreePiece(const CkCallback& cb, bool if_OCT_DECOMP) {
  if (if_OCT_DECOMP) {
    n_expected = readers.ckLocalBranch()->splitters[thisIndex].n_particles;
    n_treepieces = readers.ckLocalBranch()->splitters.size();
    tp_key = readers.ckLocalBranch()->splitters[thisIndex].tp_key;
  }
  else {
    n_expected = readers.ckLocalBranch()->splitter_counts[thisIndex];
    tp_key = readers.ckLocalBranch()->ksplitters[thisIndex];
    tp_key |= (Key)1 << (KEY_BITS-1); // add placeholder bit
  }
  cur_idx = 0;
  contribute(cb);
}

/*
void TreePiece::initialize(const CkCallback& cb) {
  n_expected = readers.ckLocalBranch()->splitter_counts[thisIndex];
  tp_key = readers.ckLocalBranch()->splitters[thisIndex];
  tp_key |= (Key)1 << (KEY_BITS-1); // add placeholder bit
  cur_idx = 0;

  contribute(cb);
}*/


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
  // create global root and recurse
  root = new Node(1, 0, &particles[0], particles.size(), 0, n_treepieces - 1, NULL);
  recursiveBuild(root, false);

  /*
#ifdef DEBUG
  //print(root);
  Node* cur = root;
  int sib = 0;
  std::cout << "[TP " << thisIndex << "] key: " << std::bitset<64>(tp_key) << " particles: " << particles.size() << std::endl;
  while (1) {
    std::cout << "[Level " << cur->depth << "] cur: " << std::bitset<64>(cur->key) << ", " << cur->n_particles << " particles" << std::endl;
    if (cur->parent == NULL) { // root
      if (cur->children.size() > 0) {
        // go down one level
        cur = cur->children[0];
      }
      else {
        break;
      }
    }
    else {
      sib++;
      if (sib < cur->parent->children.size()) {
        // visit sibling
        cur = cur->parent->children[sib];
      }
      else {
        if (cur->children.size() > 0) {
          // go down one level
          cur = cur->children[0];
        }
        else {
          break;
        }
      }
    }
  }
#endif
*/

  contribute(cb);
}

bool TreePiece::recursiveBuild(Node* node, bool saw_tp_key) {
  // store reference to splitters
  CkVec<Splitter>& splitters = readers.ckLocalBranch()->splitters;

  if (tree_type == OCT_TREE) {
    // check if we are inside the subtree rooted at the treepiece's key
    if (!saw_tp_key) {
      saw_tp_key = (node->key == tp_key);
    }

    bool is_light = (node->n_particles <= BUCKET_TOLERANCE * max_particles_per_leaf);
    bool is_prefix = Utility::isPrefix(node->key, tp_key);
    int owner_start = node->owner_tp_start;
    int owner_end = node->owner_tp_end;
    bool single_owner = (owner_start == owner_end);

    if (is_light) {
      if (saw_tp_key) {
        // we can make the node a local leaf
        if (node->n_particles == 0)
          node->type = Node::EmptyLeaf;
        else
          node->type = Node::Leaf;

        return true;
      }
      else if (!is_prefix) {
        // we have diverged from the path to the subtree rooted at the treepiece's key
        // so designate as remote
        CkAssert(node->n_particles == 0);
        CkAssert(node->n_children == 0);

        if (single_owner) {
          int assigned = splitters[owner_start].n_particles;
          if (assigned == 0) {
            node->type = Node::RemoteEmptyLeaf;
          }
          else if (assigned <= BUCKET_TOLERANCE * max_particles_per_leaf) {
            node->type = Node::RemoteLeaf;
          }
          else {
            node->type = Node::Remote;
            node->n_children = BRANCH_FACTOR;
          }
        }
        else {
          node->type = Node::Remote;
          node->n_children = BRANCH_FACTOR;
        }

        return false;
      }
    }

    // create children
    node->n_children = BRANCH_FACTOR;
    Key child_key = node->key << LOG_BRANCH_FACTOR;
    int start = 0;
    int finish = start + node->n_particles;
    int non_local_children = 0;

    for (int i = 0; i < node->n_children; i++) {
      Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1);

      // find number of particles in child
      int first_ge_idx;
      if (i < node->n_children - 1) {
        first_ge_idx = Utility::binarySearchGE(sibling_splitter, node->particles, start, finish);
      }
      else {
        first_ge_idx = finish;
      }
      int n_particles = first_ge_idx - start;

      // find owner treepieces of child
      int child_owner_start = owner_start;
      int child_owner_end;
      if (single_owner) {
        child_owner_end = child_owner_start;
      }
      else {
        if (i < node->n_children - 1) {
          int first_ge_tp = Utility::binarySearchGE(sibling_splitter, &splitters[0], owner_start, owner_end);
          child_owner_end = first_ge_tp - 1;
          owner_start = first_ge_tp;
        }
        else {
          child_owner_end = owner_end;
        }
      }

      // create child and store in vector
      Node* child = new Node(child_key, node->depth + 1, node->particles + start, n_particles, child_owner_start, child_owner_end, node);
      node->children.push_back(child);

      // recursive tree build
      bool local = recursiveBuild(child, saw_tp_key);

      if (!local) {
        non_local_children++;
      }

      start = first_ge_idx;
      child_key++;
    }

    if (non_local_children == 0) {
      node->type = Node::Internal;
    }
    else {
      node->type = Node::Boundary;
    }

    return (non_local_children == 0);
  }
}

void TreePiece::print(Node* root) {
  ostringstream oss;
  oss << "tree." << thisIndex << ".dot";
  ofstream out(oss.str().c_str());
  CkAssert(out.is_open());
  out << "digraph tree" << thisIndex << "{" << endl;
  root->dot(out);
  out << "}" << endl;
  out.close();
}
