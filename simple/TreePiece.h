#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "templates.h"
#include "Particle.h"
#include "Node.h"
#include "Utility.h"
#include "Reader.h"
#include "CentroidVisitor.h"
#include "GravityVisitor.h"
//#include "TEHolder.h"

#include <queue>
#include <map>
#include <stack>
#include <queue>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>

template <typename Data>
struct DownTraversal {
  Vector3D<Real> sum_force;
  std::vector<Node<Data>*> curr_nodes;
  enum Status {Active, Waiting, Done};
  Status status;

  DownTraversal(): sum_force(0), curr_nodes(std::vector<Node<Data>*>()), status(DownTraversal<Data>::Active) {}
};

extern CProxy_Reader readers;
extern int max_particles_per_leaf;
extern int decomp_type;
extern int tree_type;
extern CProxy_TreeElement<CentroidData> centroid_calculator;
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
  std::vector<DownTraversal<Data>> down_traversals;
  CProxy_TreeElement<Data> global_data;

  template <typename Visitor>
  void goDown();

public:
  TreePiece(const CkCallback&, int, int); 
  void receive(ParticleMsg*);
  void check(const CkCallback&);
  void triggerRequest();
  void build(const CkCallback&);
  bool recursiveBuild(Node<Data>*, bool);
  template<typename Visitor>
  void upOnly();
  template<typename Visitor>
  void startDown(TEHolder<Data>);
  template<typename Visitor>
  void requestNodes(Key, int);
  template<typename Visitor>
  void addCache(Node<Data>); 
  void print(Node<Data>*);
  Node<Data>* findNode(Key);
  void perturb (Real timestep);
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
  CkPrintf("[TP %d] key: 0x%" PRIx64 " particles: %d\n", this->thisIndex, tp_key, particles.size());
  root = new Node<Data>(1, 0, &particles[0], particles.size(), 0, n_treepieces - 1, NULL);
  recursiveBuild(root, false);

  this->contribute(cb);
}

template <typename Data>
bool TreePiece<Data>::recursiveBuild(Node<Data>* node, bool saw_tp_key) {
#if DEBUG
  CkPrintf("[Level %d] created node 0x%" PRIx64 " with %d particles\n",
      node->depth, node->key, node->n_particles);
#endif
  // store reference to splitters
  //static std::vector<Splitter>& splitters = readers.ckLocalBranch()->splitters;

  if (tree_type == OCT_TREE) {
    // check if we are inside the subtree rooted at the treepiece's key
    if (!saw_tp_key) {
      saw_tp_key = (node->key == tp_key);
    }

    bool is_light = (node->n_particles <= ceil(BUCKET_TOLERANCE * max_particles_per_leaf));
    bool is_prefix = Utility::isPrefix(node->key, tp_key);
    /*
    int owner_start = node->owner_tp_start;
    int owner_end = node->owner_tp_end;
    bool single_owner = (owner_start == owner_end);
    */

    if (saw_tp_key) {
      // XXX if we are under the TP key, we can stop going deeper if node is light
      if (is_light) {
        if (node->n_particles == 0)
          node->type = Node<Data>::EmptyLeaf;
        else
          node->type = Node<Data>::Leaf;

        return true;
      }
    }
    else { // still on the way to TP, we could have diverged
      if (!is_prefix) {
        // diverged, should be remote
        node->type = Node<Data>::Remote;

        return false;
      }
    }

    // for all other cases, we go deeper

    /*
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
        node->type = Node<Data>::Remote;

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
      else {
        CkAbort("Error: can a light node be an internal node (above a TP root)?");
      }
    }
    */

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

      /*
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
      */

      // create child and store in vector
      Node<Data>* child = new Node<Data>(child_key, node->depth + 1, node->particles + start, n_particles, 0, n_treepieces - 1, node);
      //Node<Data>* child = new Node<Data>(child_key, node->depth + 1, node->particles + start, n_particles, child_owner_start, child_owner_end, node);
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
  Visitor v (this->thisProxy, this->thisIndex); // weird?
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
template <typename Visitor>
void TreePiece<Data>::startDown(TEHolder<Data> te_holderi) {
  global_data = te_holderi.te_proxy;
  down_traversals = std::vector<DownTraversal<Data>> (particles.size());
  for (int i = 0; i < down_traversals.size(); i++) {
    down_traversals[i].curr_nodes.push_back(root);
  }
  goDown<Visitor>();
}

template<typename Data>
Node<Data>* TreePiece<Data>::findNode(Key key) {
  Node<Data>* node = root;
  while (node->key != key) {
    Key diff = key - node->key;
    while (diff >= 8) diff /= 8;
    node = node->children[diff];
  }
  return node;
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::requestNodes(Key key, int index) {
  Node<Data>* node = findNode(key);
  this->thisProxy[index].template addCache<Visitor>(*node);

}

template<typename Data>
template<typename Visitor>
void TreePiece<Data>::addCache(Node<Data> new_node) {
  Node<Data>* node = findNode(new_node.key); 
  if (new_node.type == Node<Data>::Boundary) {
    node->data = new_node.data;
    node->type = Node<Data>::CachedBoundary;
  }
  else if (new_node.type == Node<Data>::Leaf) {
    node->n_particles = new_node.n_particles;
    node->particles = new Particle [node->n_particles];
    for (int i = 0; i < node->n_particles; i++) {
      node->particles[i] = new_node.particles[i];
    }
    node->type = Node<Data>::CachedRemoteLeaf;
  }
  else {
    node->n_children = 8;
    for (int i = 0; i < 8; i++) {
      Node<Data>* new_child = new Node<Data> (node->key * 8 + i, node->depth+1, NULL, 0, 0, 0, node);
      new_child->type = Node<Data>::Remote; // RemoteLeaf necessary? nah idts
      node->children.push_back(new_child); 
    }
    node->type = Node<Data>::CachedRemote;
  }
  Key key = new_node.key;
  for (int i = 0; i < down_traversals.size(); i++) {
    DownTraversal<Data>& dt = down_traversals[i];
    if (dt.status == DownTraversal<Data>::Waiting) {
      for (int j = 0; j < dt.curr_nodes.size(); j++) {
        if (dt.curr_nodes[j]->key == new_node.key) dt.status = DownTraversal<Data>::Active;
      }
    }
  }
  goDown<Visitor>();
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::goDown() {
  Visitor v;
  for (int i = 0; i < down_traversals.size(); i++) if (down_traversals[i].status == DownTraversal<Data>::Active) {
    DownTraversal<Data>& dt = down_traversals[i];
    std::stack<Node<Data>*> nodes;
    std::vector<Node<Data>*> next_nodes;
    for (int j = 0; j < dt.curr_nodes.size(); j++) {
      if (dt.curr_nodes[j]->type != Node<Data>::Remote || dt.curr_nodes[j]->type != Node<Data>::Boundary) { // wrong
        nodes.push(dt.curr_nodes[j]);
      }
      else {
        next_nodes.push_back(dt.curr_nodes[j]);
      }
    }
    while (nodes.size()) {
      Node<Data>* node = nodes.top();
      nodes.pop();
      switch (node->type) {
        case Node<Data>::CachedBoundary:
        case Node<Data>::CachedRemote:
        case Node<Data>::Internal: {
          if (v.node(node, particles[i], down_traversals[i].sum_force)) {
            for (int i = 0; i < node->children.size(); i++) {
              nodes.push(node->children[i]);
            }
          }
          break;
        }
        case Node<Data>::CachedRemoteLeaf:
        case Node<Data>::Leaf: {
          v.leaf(node, particles[i], down_traversals[i].sum_force);
          break;
        }
        case Node<Data>::Boundary: {
          global_data[node->key].template requestData<Visitor>(this->thisIndex);
          next_nodes.push_back(node);
          break;
        }
        case Node<Data>::Remote:
        case Node<Data>::RemoteLeaf: {
          Node<Data>* temp = node;
          while (temp->parent->type != Node<Data>::CachedBoundary) temp = temp->parent;
          Key key = temp->key; // might need to be temp->parent->key? idk
          global_data[key].template requestTP<Visitor>(node->key, this->thisIndex);
          next_nodes.push_back(node);
          break;
        }
      }
    }
    dt.curr_nodes = next_nodes;
    dt.status = (dt.curr_nodes.size()) ? DownTraversal<Data>::Waiting : DownTraversal<Data>::Done;
  }
}

template <typename Data>
void TreePiece<Data>::perturb (Real timestep) {
  for (int i = 0; i < particles.size(); i++) {
    particles[i].perturb(timestep, down_traversals[i].sum_force);
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

