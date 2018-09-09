#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "templates.h"
#include "ParticleMsg.h"
#include "Node.h"
#include "Utility.h"
#include "Reader.h"
#include "CacheManager.h"
#include "Resumer.h"
#include "Traverser.h"
#include "Driver.h"

#include <queue>
#include <set>
#include <stack>
#include <queue>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <atomic>
#include <mutex>
#include <bitset>

extern CProxy_Reader readers;
extern int max_particles_per_leaf;
extern int decomp_type;
extern int tree_type;
extern CProxy_Main mainProxy;

template <typename Data>
class TreePiece : public CBase_TreePiece<Data> {
public:
  std::vector<Particle> particles;
  std::vector<Node<Data>*> leaves;
  std::vector<Node<Data>*> empty_leaves;
  int n_total_particles;
  int n_treepieces;
  int particle_index;
  int n_expected;
  Key tp_key; // should be a prefix of all particle keys underneath this node
  Node<Data>* root;
  Node<Data>* root_from_tp_key;
  Traverser<Data>* traverser;
  std::vector<std::pair<Node<Data>*, int>> local_travs;
  CProxy_TreeElement<Data> global_data;
  CProxy_CacheManager<Data> cache_manager;
  CacheManager<Data>* cache_local;
  CProxy_Resumer<Data> resumer;
  std::vector<std::vector<Node<Data>*>> interactions;
  bool cache_init;
  // debug
  std::vector<Particle> flushed_particles;

  TreePiece(const CkCallback&, int, int, CProxy_TreeElement<Data>,
    CProxy_Resumer<Data>, CProxy_CacheManager<Data>, CProxy_Driver<Data>);
  void receive(ParticleMsg*);
  void check(const CkCallback&);
  void triggerRequest();
  void build(bool to_search);
  bool recursiveBuild(Node<Data>*, bool);
  void upOnly();
  inline void initCache();
  void requestNodes(Key, int);
  template<typename Visitor> void startDown();
  template<typename Visitor> void startUpAndDown();
  template<typename Visitor> void startDual(Key*, int);
  void goDown(Key); 
  void processLocal(const CkCallback&);
  void interact(const CkCallback&);
  void print(Node<Data>*);
  void perturb (Real timestep);
  void flush(CProxy_Reader);
  void rebuild(bool to_search);

  // debug
  void checkParticlesChanged(const CkCallback& cb) {
    bool result = true;
    if (particles.size() != flushed_particles.size()) {
      result = false;
      this->contribute(sizeof(bool), &result, CkReduction::logical_and_bool, cb);
      return;
    }
    for (int i = 0; i < particles.size(); i++) {
      if (!(particles[i] == flushed_particles[i])) {
        result = false;
        break;
      }
    }
    this->contribute(sizeof(bool), &result, CkReduction::logical_and_bool, cb);
  }
};
template <typename Data>
TreePiece<Data>::TreePiece(const CkCallback& cb, int n_total_particles_, int n_treepieces_, CProxy_TreeElement<Data> global_datai, CProxy_Resumer<Data> resumeri, CProxy_CacheManager<Data> cache_manageri, CProxy_Driver<Data> driver) : n_total_particles(n_total_particles_), n_treepieces(n_treepieces_), particle_index(0) {
  global_data = global_datai;
  resumer = resumeri;
  resumer.ckLocalBranch()->tp_proxy = this->thisProxy;
  cache_manager = cache_manageri;
  cache_local = cache_manager.ckLocalBranch();
  resumer.ckLocalBranch()->cache_local = cache_local;
  cache_local->resumer = resumer;
  cache_init = false;

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
  global_data[tp_key].recvProxies(TPHolder<Data>(this->thisProxy), this->thisIndex, cache_manager, driver);
  Key temp = tp_key;
  while (temp > 0 && temp % 8 == 0) {
    temp /= 8;
    //CkPrintf("temp = %d\n", temp);
    global_data[temp].recvProxies(TPHolder<Data>(this->thisProxy), -1, cache_manager, driver);
 }
  this->contribute(cb);
  root_from_tp_key = nullptr;
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
void TreePiece<Data>::build(bool to_search) {
  // sort particles received from readers
  std::sort(particles.begin(), particles.end());
  // create global root and recurse
#ifdef DEBUG
  CkPrintf("[TP %d] key: 0x%" PRIx64 " particles: %d\n", this->thisIndex, tp_key, particles.size());
#endif
  root = new Node<Data>(1, 0, particles.size(), &particles[0], 0, n_treepieces - 1, nullptr);
  recursiveBuild(root, false);
  interactions = std::vector<vector<Node<Data>*>> (leaves.size());
  if (to_search) initCache();
  upOnly();
}
template <typename Data>
bool TreePiece<Data>::recursiveBuild(Node<Data>* node, bool saw_tp_key) {
#if DEBUG
  //CkPrintf("[Level %d] created node 0x%" PRIx64 " with %d particles\n",
    //  node->depth, node->key, node->n_particles);
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
        if (node->n_particles == 0) {
          node->type = Node<Data>::EmptyLeaf;
          empty_leaves.push_back(node);
        }
        else {
          node->type = Node<Data>::Leaf;
          leaves.push_back(node);
        }
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
      Node<Data>* child = new Node<Data>(child_key, node->depth + 1, n_particles, node->particles + start, 0, n_treepieces - 1, node);
      //Node<Data>* child = new Node<Data>(child_key, node->depth + 1, node->particles + start, n_particles, child_owner_start, child_owner_end, node);
      node->children[i].store(child);

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
  return false;
}
template <typename Data>
void TreePiece<Data>::upOnly() {
  std::queue<Node<Data>*> going_up;
  for (auto leaf : leaves) {
    leaf->data = Data(leaf->particles, leaf->n_particles);
    going_up.push(leaf);
  }
  if (!leaves.size()) going_up.push(root_from_tp_key);
  else for (auto empty_leaf : empty_leaves) going_up.push(empty_leaf);
  while (going_up.size()) {
    Node<Data>* node = going_up.front();
    going_up.pop();
    if (node->key == tp_key) {
      global_data[tp_key >> 3].recvData(node->data, true);
    }
    else {
      Node<Data>* parent = node->parent;
      parent->data += node->data;
      parent->wait_count--;
      if (parent->wait_count == 0) going_up.push(parent); 
    }
  }
}
template <typename Data>
void TreePiece<Data>::initCache() {
  if (!cache_init) {
    root_from_tp_key = root->findNode(tp_key);
    cache_local->connect(root_from_tp_key, false);
    root_from_tp_key->parent->children[tp_key % 8].store(nullptr);
    root->triggerFree();
    cache_init = true;
  }
}
template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startDown() {
  traverser = new DownTraverser<Data, Visitor>(this);
  goDown(1);
}
template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startUpAndDown() {
  if (!leaves.size()) return;
  traverser = new UpnDTraverser<Data, Visitor>(this);
  for (auto leaf : leaves) goDown(leaf->key);
}
template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startDual(Key* keys_ptr, int n) {
  std::vector<Key> keys (keys_ptr, keys_ptr + n);
  traverser = new DualTraverser<Data, Visitor>(this, keys);
  for (auto key : keys) goDown(key);
  // root needs to be the root of the searched tree, not the searching tree
}
template <typename Data>
void TreePiece<Data>::requestNodes(Key key, int cm_index) {
  Node<Data>* node = root_from_tp_key->findNode(key);
  if (!node) CkPrintf("null found for key %d on tp %d\n", key, this->thisIndex);
  cache_local->serviceRequest(node, cm_index);
}
template <typename Data>
void TreePiece<Data>::goDown(Key new_key) {
  traverser->template traverse(new_key);
}
template <typename Data>
void TreePiece<Data>::processLocal(const CkCallback& cb) {
  traverser->processLocal();
  this->contribute(cb);
}
template <typename Data>
void TreePiece<Data>::interact(const CkCallback& cb) {
  traverser->interact();
  this->contribute(cb);
}
template <typename Data>
void TreePiece<Data>::perturb (Real timestep) {
  for (auto leaf : leaves) {
    for (int i = 0; i < leaf->n_particles; i++) {
      leaf->particles[i].perturb(timestep, leaf->sum_forces[i]);
    }
  }
}
template <typename Data>
void TreePiece<Data>::flush(CProxy_Reader readers) {
  // debug
  flushed_particles.resize(0);
  flushed_particles.insert(flushed_particles.end(), particles.begin(), particles.end());

  ParticleMsg *msg = new (particles.size()) ParticleMsg(particles.data(), particles.size());
  readers[CkMyPe()].receive(msg);
  particles.resize(0);
  particle_index = 0;
}
template <typename Data>
void TreePiece<Data>::rebuild(bool to_search) {
  // clean up old tree information
  root->triggerFree();
  root = nullptr;

  // build a new tree from scratch
  build(to_search);
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
