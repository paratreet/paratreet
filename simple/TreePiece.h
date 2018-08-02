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
extern CProxy_Main mainProxy;

template <typename Data>
class TreePiece : public CBase_TreePiece<Data> {
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
  std::unordered_map<Key, std::vector<int>> curr_nodes;
  std::vector<int> num_waiting;
  std::vector<Node<Data>*> trav_tops;
  CProxy_TreeElement<Data> global_data;
  CProxy_CacheManager<Data> cache_manager;
  CacheManager<Data>* cache_local;
  CProxy_Resumer<Data> resumer;
  CkCallback current_cb;
  bool cache_init;
  int num_done;
  // debug
  std::vector<Particle> flushed_particles;

public:
  TreePiece(const CkCallback&, int, int, CProxy_TreeElement<Data>, CProxy_Resumer<Data>, CProxy_CacheManager<Data>);
  void receive(ParticleMsg*);
  void check(const CkCallback&);
  void triggerRequest();
  void build(CkCallback);
  bool recursiveBuild(Node<Data>*, bool);
  void upOnly();
  inline void initCache(CkCallback);
  template<typename Visitor>
  void initProcessor();
  template<typename Visitor>
  void startDown(CkCallback);
  template<typename Visitor>
  void startUpAndDown(CkCallback);
  void requestNodes(Key, int);
  template<typename Visitor>
  void goDown(Key); 
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
TreePiece<Data>::TreePiece(const CkCallback& cb, int n_total_particles_, int n_treepieces_, CProxy_TreeElement<Data> global_datai, CProxy_Resumer<Data> resumeri, CProxy_CacheManager<Data> cache_manageri) : n_total_particles(n_total_particles_), n_treepieces(n_treepieces_), particle_index(0) {
  global_data = global_datai;
  resumer = resumeri;
  resumer.ckLocalBranch()->tp_proxy = this->thisProxy;
  cache_manager = cache_manageri;
  cache_local = cache_manager.ckLocalBranch();
  resumer.ckLocalBranch()->cache_local = cache_local;
  cache_local->resumer = resumer;
  if (cache_local->first_pe > -1 && cache_local->first_pe != CkMyPe())
    cache_local->isNG = true;
  cache_local->first_pe = CkMyPe();
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

  global_data[tp_key].receiveProxies(TPHolder<Data>(this->thisProxy), this->thisIndex, cache_manager);
  Key temp = tp_key;
  while (temp > 0 && temp % 8 == 0) {
    temp /= 8;
    //CkPrintf("temp = %d\n", temp);
    global_data[temp].receiveProxies(TPHolder<Data>(this->thisProxy), -1, cache_manager);
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
void TreePiece<Data>::build(CkCallback cb) {
  // sort particles received from readers
  std::sort(particles.begin(), particles.end());

  // create global root and recurse
#ifdef DEBUG
  CkPrintf("[TP %d] key: 0x%" PRIx64 " particles: %d\n", this->thisIndex, tp_key, particles.size());
#endif
  root = new Node<Data>(1, 0, particles.size(), &particles[0], 0, n_treepieces - 1, nullptr);
  recursiveBuild(root, false);

  initCache(cb);
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
      global_data[tp_key >> 3].receiveData(node->data);
    }
    else {
      Node<Data>* parent = node->parent;
      parent->data = parent->data + node->data;
      parent->wait_count--;
      if (parent->wait_count == 0) going_up.push(parent); 
    }
  }
}

template <typename Data>
void TreePiece<Data>::initCache(CkCallback cbi) {
  if (!cache_init) {
    cache_local->build_cb = cbi;
    root_from_tp_key = root->findNode(tp_key);
    cache_local->connect(root_from_tp_key);
    root_from_tp_key->parent->children[tp_key % 8] = nullptr;
    root->triggerFree();
    cache_init = true;
  }
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::initProcessor() {
  if (!cache_local->processor_set) {
    cache_local->processor_set = true;
    cache_local->processor = [] (CProxy_Resumer<Data> rsmr, bool isNG, int cm_index, Key key) {
      if (!isNG) rsmr[cm_index].template process<Visitor> (key);
      else for (int i = 0; i < CkNodeSize(0); i++) {
        rsmr[cm_index * CkNodeSize(0) + i].template process<Visitor>(key);
      }
    };
  }
}


template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startDown(CkCallback cbi) {
  current_cb = cbi;
  initProcessor<Visitor>();
  for (int i = 0; i < leaves.size(); i++) curr_nodes[1].push_back(i);
  num_waiting = std::vector<int> (leaves.size(), 1);
  num_done = 0;
  trav_tops = std::vector<Node<Data>*> (leaves.size(), root);
  goDown<Visitor> (1);
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::startUpAndDown(CkCallback cbi) {
  current_cb = cbi;
  initProcessor<Visitor>();
  if (!leaves.size()) {
    //CkPrintf("tp %d finished!\n", this->thisIndex);
    this->contribute(current_cb);
  }
  num_waiting = std::vector<int> (leaves.size(), 1);
  num_done = 0;
  trav_tops = std::vector< Node<Data>* > (leaves.size(), nullptr);
  for (int i = 0; i < leaves.size(); i++) curr_nodes[leaves[i]->key].push_back(i);
  for (auto leaf : leaves) goDown<Visitor> (leaf->key);
}

template <typename Data>
void TreePiece<Data>::requestNodes(Key key, int cm_index) {
  Node<Data>* node = root_from_tp_key->findNode(key);
  if (!node) CkPrintf("null found for key %d on tp %d\n", key, this->thisIndex);
  cache_local->serviceRequest(node, cm_index);
}

template <typename Data>
template <typename Visitor>
void TreePiece<Data>::goDown(Key new_key) {
  Visitor v;
  if (new_key == 1) root = cache_local->root;
  auto& now_ready = curr_nodes[new_key];
  std::vector<std::pair<Key, int>> curr_nodes_insertions;
  Node<Data>* start_node = root;
  if (new_key > 1) {
    Node<Data>*& result = resumer.ckLocalBranch()->nodehash[new_key];
    if (!result) CkPrintf("not good!\n");
    if (!result) result = root->findNode(new_key);
    start_node = result;
  }
  //CkPrintf("going down on key %d while its type is %d\n", new_key, start_node->type);

  CkAssert(start_node);
  for (auto i : now_ready) {
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
          bool prev = node->requested.exchange(true);
          if (!prev) {
            if (node->type == Node<Data>::Boundary || node->type == Node<Data>::RemoteAboveTPKey)
              global_data[node->key].requestData(cache_local->thisIndex);
            else cache_manager[node->cm_index].requestNodes(std::make_pair(node->key, cache_local->thisIndex));
          }
          std::vector<int>& list = resumer.ckLocalBranch()->waiting[node->key];
          if (!list.size() || list.back() != this->thisIndex) list.push_back(this->thisIndex);
        }
      }
    } 
    if (num_waiting[i] == 0) {
      if (trav_tops[i] == nullptr) trav_tops[i] = cache_local->root;
      if (trav_tops[i]->parent == nullptr) {
        num_done++;
      }
      else {
        for (auto child : trav_tops[i]->parent->children) {
          if (child != trav_tops[i]) {
             if (trav_tops[i]->parent->type == Node<Data>::Boundary) child->type = Node<Data>::RemoteAboveTPKey;
             curr_nodes_insertions.push_back(std::make_pair(child->key, i));
             num_waiting[i]++;
             this->thisProxy[this->thisIndex].template goDown<Visitor> (child->key);
          }
        }
        trav_tops[i] = trav_tops[i]->parent;
      }
    }
    //else CkPrintf("tp %d leaf %d still waiting\n", this->thisIndex, i);
  }
  curr_nodes.erase(new_key);
  for (auto cn : curr_nodes_insertions) curr_nodes[cn.first].push_back(cn.second);
  //if (curr_nodes.size()) CkPrintf("still missing at least node %d on tp %d, leaf %d\n", curr_nodes.begin()->first, this->thisIndex, curr_nodes.begin()->second[0]);
  if (num_done == leaves.size()) {
    //CkPrintf("tp %d finished!, we got key %d\n", this->thisIndex, new_key);
    this->contribute(current_cb);
  }
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
