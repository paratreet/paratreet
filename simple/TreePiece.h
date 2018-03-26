#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "Node.h"
#include "Utility.h"

#include <queue>
#include <map>
#include <stack>
#include "DataInterface.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include "Reader.h"
#include "CentroidVisitor.h"

#include "templates.h"

extern CProxy_Reader readers;
extern int max_particles_per_leaf;
extern int decomp_type;
extern int tree_type;
extern CProxy_TreeElement<CentroidVisitor, CentroidData> centroid_calculator;
extern CProxy_Main mainProxy;

template <typename Data>
class TreePiece : public CBase_TreePiece<Data> {
  std::vector<Particle> particles;
  int n_total_particles;
  int n_treepieces;
  int particle_index;
  int n_expected;
  Key tp_key; // should be a prefix of all particle keys underneath this node
  Node<Data>* root;

public:
  TreePiece(const CkCallback&, int, int);
  void receive(ParticleMsg*);
  void check(const CkCallback&);
  void triggerRequest();
  void build(const CkCallback&);
  bool recursiveBuild(Node<Data>*, bool);
  template<typename Visitor>
  void upOnly();
  void print(Node<Data>*);

};

template <typename Data>
TreePiece<Data>::TreePiece(const CkCallback& cb, int n_total_particles_, int n_treepieces_) : n_total_particles(n_total_particles_), n_treepieces(n_treepieces_), particle_index(0) {
  if (decomp_type == OCT_DECOMP) {
    // OCT decomposition
    n_expected = readers.ckLocalBranch()->splitters[this->thisIndex].n_particles;
    tp_key = readers.ckLocalBranch()->splitters[this->thisIndex].tp_key;
  }
  else if (decomp_type == SFC_DECOMP) {
    // SFC decomposition
    n_expected = n_total_particles / n_treepieces;
    if (this->thisIndex < (n_total_particles % n_treepieces))
      n_expected++;
      // TODO tp_key needs to be found in local tree build
  }
  this->contribute(cb);
}

template <typename Data>
void TreePiece<Data>::receive(ParticleMsg* msg) {
  // copy particles to local vector
  particles.resize(particle_index + msg->n_particles);
  std::memcpy(&particles[particle_index], msg->particles, msg->n_particles * sizeof(Particle));
  particle_index += msg->n_particles;
  delete msg;
}

template <typename Data>
void TreePiece<Data>::check(const CkCallback& cb) {
  if (n_expected != particles.size()) {
    CkPrintf("[TP %d] ERROR! Only %d particles out of %d received\n", this->thisIndex, particles.size(), n_expected);
    CkAbort("Failure on receiving particles");
  }
  this->contribute(cb);
}

template <typename Data>
void TreePiece<Data>::triggerRequest() {
  readers.ckLocalBranch()->request(this->thisProxy, this->thisIndex, n_expected);
}

template <typename Data>
void TreePiece<Data>::build(const CkCallback &cb){
  // create global root and recurse
  root = new Node<Data>(1, 0, &particles[0], particles.size(), 0, n_treepieces - 1, NULL);
  recursiveBuild(root, false);

#ifdef DEBUG
  //print(root);
  Node<Data>* cur = root;
  int sib = 0;
  CkPrintf("[TP %d] key: 0x%" PRIx64 " particles: %d\n", this->thisIndex, tp_key, particles.size());
  while (1) {
    CkPrintf("[Level %d] cur: 0x%" PRIx64 " particles: %d\n", cur->depth, cur->key, cur->n_particles);
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

  this->contribute(cb);
}

template <typename Data>
bool TreePiece<Data>::recursiveBuild(Node<Data>* node, bool saw_tp_key) {
  // store reference to splitters
  static std::vector<Splitter>& splitters = readers.ckLocalBranch()->splitters;

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
          node->type = Node<Data>::EmptyLeaf;
        else
          node->type = Node<Data>::Leaf;

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
            node->type = Node<Data>::RemoteEmptyLeaf;
          }
          else if (assigned <= BUCKET_TOLERANCE * max_particles_per_leaf) {
            node->type = Node<Data>::RemoteLeaf;
          }
          else {
            node->type = Node<Data>::Remote;
            node->n_children = BRANCH_FACTOR;
          }
        }
        else {
          node->type = Node<Data>::Remote;
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
      Node<Data>* child = new Node<Data>(child_key, node->depth + 1, node->particles + start, n_particles, child_owner_start, child_owner_end, node);
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
      node->type = Node<Data>::Internal;
    }
    else {
      node->type = Node<Data>::Boundary;
    }

    return (non_local_children == 0);
  }
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::upOnly() {
  std::queue<Node<Data>*> nodes;
  nodes.push(root);
  Visitor v (tp_key);
  int n_curr_particles = 0;
  while (nodes.size()) {
    Node<Data>* node = nodes.front();
    nodes.pop();
    if (n_curr_particles < n_total_particles) { // on way down
      node->wait_count = 0;
      if (node->type == Node<Data>::Internal || node->type == Node<Data>::Boundary) {
        for (int i = 0; i < node->children.size(); i++) {
          nodes.push(node->children[i]);
          switch (node->children[i]->type) {
            case Node<Data>::Internal:
            case Node<Data>::Boundary:
            case Node<Data>::Leaf:
              node->wait_count++;
          }
        }
      }
    }
    if (node->type == Node<Data>::Leaf) {
      v.leaf(node);
      node->wait_count = 0;
      n_curr_particles += node->n_particles;
    }
    if (node->wait_count == 0) {
      if (v.node(node)) { 
        if (--node->parent->wait_count == 0) nodes.push(node->parent);
      }
    }
  }
}

template <typename Data>
void TreePiece<Data>::print(Node<Data>* root) {
  ostringstream oss;
  oss << "tree." << this->thisIndex << ".dot";
  ofstream out(oss.str().c_str());
  CkAssert(out.is_open());
  out << "digraph tree" << this->thisIndex << "{" << endl;
  root->dot(out);
  out << "}" << endl;
  out.close();
}

#endif // SIMPLE_TREEPIECE_H_

