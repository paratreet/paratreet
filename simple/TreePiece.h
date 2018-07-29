#ifndef SIMPLE_TREEPIECE_H_
#define SIMPLE_TREEPIECE_H_

#include "simple.decl.h"
#include "common.h"
#include "templates.h"
#include "ParticleMsg.h"
#include "MultiMsg.h"
#include "Node.h"
#include "Utility.h"
#include "Reader.h"
#include "CentroidVisitor.h"
#include "GravityVisitor.h"
#include "CacheManager.h"

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

extern CProxy_Reader readers;
extern int max_particles_per_leaf;
extern int decomp_type;
extern int tree_type;
extern CProxy_TreeElement<CentroidData> centroid_calculator;
extern CProxy_CacheManager<CentroidData> centroid_cache;
extern CProxy_Main mainProxy;

template <typename Data>
class TreePiece : public CBase_TreePiece<Data> {
  std::vector<Particle> particles;
  std::vector<Node<Data>*> leaves;
  int n_total_particles;
  int n_treepieces;
  int particle_index;
  int n_expected;
  Key tp_key; // should be a prefix of all particle keys underneath this node
  Node<Data>* root;
  Node<Data>* root_from_tp_key;
  std::unordered_multimap<Key, int> curr_nodes;
  std::vector<int> num_waiting;
  std::vector<Node<Data>*> trav_tops;
  CProxy_TreeElement<Data> global_data;
  CProxy_CacheManager<Data> cache_manager;
  int num_done;
  // debug
  std::vector<Particle> flushed_particles;

public:
  TreePiece(const CkCallback&, int, int); 
  void receive(ParticleMsg*);
  void check(const CkCallback&);
  void triggerRequest();
  void build(const CkCallback&);
  bool recursiveBuild(Node<Data>*, bool);
  template<typename Visitor>
  void upOnly(CProxy_TreeElement<Data>);
  void initCache(CProxy_CacheManager<Data>);
  template<typename Visitor>
  void initCache(CProxy_CacheManager<Data>);
  template<typename Visitor>
  void startDown(CProxy_CacheManager<Data>);
  template<typename Visitor>
  void startUpAndDown(CProxy_CacheManager<Data>);
  void requestNodes(Key, CProxy_CacheManager<Data>, int);
  template<typename Visitor>
  void goDown(Key); 
  template <typename Visitor>
  void catchMissed();
  void print(Node<Data>*);
  void perturb (Real timestep);
  void flush(CProxy_Reader);
  void rebuild(const CkCallback&);

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
void TreePiece<Data>::build(const CkCallback &cb){
  // sort particles received from readers
  std::sort(particles.begin(), particles.end());

  // create global root and recurse
#ifdef DEBUG
  CkPrintf("[TP %d] key: 0x%" PRIx64 " particles: %d\n", this->thisIndex, tp_key, particles.size());
#endif
  root = new Node<Data>(1, 0, particles.size(), &particles[0], 0, n_treepieces - 1, nullptr);
  recursiveBuild(root, false);

  this->contribute(cb);
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
        if (node->n_particles == 0)
          node->type = Node<Data>::EmptyLeaf;
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
      node->children[i] = child;

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
template <typename Visitor>
void TreePiece<Data>::upOnly(CProxy_TreeElement<Data> te_proxy) {
  global_data = te_proxy;
  std::queue<Node<Data>*> down, up;
  down.push(root);
  Visitor v (this->thisProxy, this->thisIndex);
  int n_curr_particles = 0;
  while (down.size()) {
    Node<Data>* node = down.front();
    down.pop();
    node->wait_count = 0;
    if (node->type == Node<Data>::Leaf) {
      v.leaf(node);
      node->wait_count = 0;
      n_curr_particles += node->n_particles;
      up.push(node);
      continue;
    }
    for (auto child : node->children) {
      if (!child) continue;
      switch (child->type) {
        case Node<Data>::Internal: case Node<Data>::Boundary: case Node<Data>::Leaf: {
          down.push(child);
          node->wait_count++;
        }
        default: break;
      }
    }
  }
  if (!up.size()) { // no Data bc no Leaves, so send blank
    global_data[tp_key].template receiveData<Visitor>(this->thisProxy, Data(), this->thisIndex);
  }
  while (up.size()) {
    Node<Data>* node = up.front();
    up.pop();
    if (v.node(node)) { 
      if (--node->parent->wait_count == 0) up.push(node->parent);
    }
  }
}

template <typename Data>
void TreePiece<Data>::initCache(CProxy_CacheManager<Data> cache_manageri) {
  cache_manager = cache_manageri;
  if (!root_from_tp_key) {
    root_from_tp_key = root->findNode(tp_key);
    cache_manager.ckLocalBranch()->connect(root_from_tp_key);
    root_from_tp_key->parent->children[tp_key % 8] = nullptr;
    root->triggerFree();
  }
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::initCache(CProxy_CacheManager<Data> cache_manageri) {
  initCache(cache_manageri);
  CacheManager<Data>* local_branch = cache_manager.ckLocalBranch();
  if (!local_branch->processor_set) {
    local_branch->processor_set = true;
    local_branch->processor = [](CacheManager<Data>* cm) {cm->template resumeTraversals<Visitor>();};
    local_branch->processor(local_branch);
  }
}


template <typename Data>
template <typename Visitor>

void TreePiece<Data>::startDown(CProxy_CacheManager<Data> cache_manageri) {
  initCache<Visitor>(cache_manageri);
  for (int i = 0; i < leaves.size(); i++) curr_nodes.insert(std::make_pair(1, i));
  num_waiting = std::vector<int> (leaves.size(), 1);
  num_done = 0;
  trav_tops = std::vector<Node<Data>*> (leaves.size(), root);
  goDown<Visitor> (1);
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startUpAndDown(CProxy_CacheManager<Data> cache_manageri) {
  initCache<Visitor>(cache_manageri);
  if (!leaves.size()) {
    CkPrintf("tp %d finished!\n", this->thisIndex);
    CkCallback cb(CkReductionTarget(Main, doneDown), mainProxy);
    this->contribute(cb);
  }
  num_waiting = std::vector<int> (leaves.size(), 1);
  num_done = 0;
  trav_tops = std::vector< Node<Data>* > (leaves.size(), nullptr);
  for (int i = 0; i < leaves.size(); i++) curr_nodes.insert(std::make_pair(leaves[i]->key, i));
  for (auto leaf : leaves) goDown<Visitor> (leaf->key);
}

template <typename Data>
void TreePiece<Data>::requestNodes(Key key, CProxy_CacheManager<Data> cache_manageri, int cm_index) {
  initCache(cache_manager);
  Node<Data>* node = root_from_tp_key->findNode(key);
  if (!node) CkPrintf("null found for key %d on tp %d\n", key, this->thisIndex);
  cache_manager.ckLocalBranch()->serviceRequest(node, cm_index);
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::goDown(Key new_key) {
  //CkPrintf("going down on key %d while its type is %d\n", new_key, findNode(new_key)->type);
  Visitor v;
  if (new_key == 1) root = cache_manager.ckLocalBranch()->root;
  std::set<Key> to_go_down;
  auto range = curr_nodes.equal_range(new_key);
  std::vector<std::pair<Key, int>> curr_nodes_insertions;
  Node<Data>* start_node = root->findNode(new_key);
  for (auto it = range.first; it != range.second; it++) {
    int i = it->second;
    num_waiting[i]--;
    std::stack<Node<Data>*> nodes;
    nodes.push(start_node);
    while (nodes.size()) {
      Node<Data>* node = nodes.top();
      nodes.pop();
      //CkPrintf("tp %d, key = %d, type = %d, pe %d\n", this->thisIndex, node->key, node->type, CkMyPe());
      switch (node->type) {
        case Node<Data>::CachedBoundary: case Node<Data>::CachedRemote: case Node<Data>::Internal: {
          if (v.node(node, leaves[i])) {
            for (int i = 0; i < node->children.size(); i++) {
              nodes.push(node->children[i]);
            }
          }
          break;
        }
        case Node<Data>::CachedRemoteLeaf: case Node<Data>::Leaf: {
          v.leaf(node, leaves[i]);
          break;
        }
        case Node<Data>::Boundary: case Node<Data>::RemoteAboveTPKey: case Node<Data>::Remote: case Node<Data>::RemoteLeaf: {
          curr_nodes_insertions.push_back(std::make_pair(node->key, i));
          num_waiting[i]++;
          bool should_send = false;
#ifdef SMPCACHE
          node->qlock.lock();
#endif
          if (!node->waiting.size()) should_send = true;
          node->waiting.insert(this->thisIndex);
#ifdef SMPCACHE
          node->qlock.unlock();
#endif
          if (should_send) {
            if (node->type == Node<Data>::Boundary || node->type == Node<Data>::RemoteAboveTPKey)
              global_data[node->key].requestData(cache_manager, cache_manager.ckLocalBranch()->thisIndex);
            else cache_manager[node->cm_index].requestNodes(std::make_pair(node->key, cache_manager.ckLocalBranch()->thisIndex));
          }

        }
      }
    } 
    if (num_waiting[i] == 0) {
      if (trav_tops[i] == nullptr) trav_tops[i] = cache_manager.ckLocalBranch()->root;
      if (trav_tops[i]->parent == nullptr) {
        num_done++;
      }
      else {        
        for (auto child : trav_tops[i]->parent->children) {
          if (child != trav_tops[i]) {
             if (trav_tops[i]->parent->type == Node<Data>::Boundary) child->type = Node<Data>::RemoteAboveTPKey;
             curr_nodes_insertions.push_back(std::make_pair(child->key, i));
             num_waiting[i]++;
             to_go_down.insert(child->key);
          }
        }
        trav_tops[i] = trav_tops[i]->parent;
      }
    }
    //else CkPrintf("TP %d still waiting on at least key %d\n", this->thisIndex, *curr_nodes[i].begin());
  }
  curr_nodes.erase(new_key);
  curr_nodes.insert(curr_nodes_insertions.begin(), curr_nodes_insertions.end());
  if (num_done == leaves.size()) {
    //CkPrintf("tp %d finished!, we got key %d\n", this->thisIndex, new_key);
    CkCallback cb(CkReductionTarget(Main, doneDown), mainProxy);
    this->contribute(cb);
  }
  for (auto to_go : to_go_down) this->thisProxy[this->thisIndex].template goDown<Visitor> (to_go);
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::catchMissed() {
  bool if_caught = curr_nodes.size();
  for (auto caught : curr_nodes) {
    CkPrintf("caught node %d missed on tp %d, leaf %d\n", caught, this->thisIndex, caught.second);
    this->thisProxy[this->thisIndex].template goDown<Visitor> (caught.first);
  }
  if (if_caught) CkStartQD(CkCallback(CkIndex_Main::catchDown(), mainProxy));
}

template <typename Data>
void TreePiece<Data>::perturb (Real timestep) {
  for (auto leaf : leaves) {
    for (int i = 0; i < leaf->n_particles; i++) {
      leaf->particles[i].perturb(timestep, leaf->data.sum_forces[i]);
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
void TreePiece<Data>::rebuild(const CkCallback& cb) {
  // clean up old tree information
  root->triggerFree();
  root = nullptr;

  // build a new tree from scratch
  build(cb);
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
