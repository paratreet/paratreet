#ifndef PARATREET_CACHEMANAGER_H_
#define PARATREET_CACHEMANAGER_H_

#include "paratreet.decl.h"
#include "common.h"
#include "MultiMsg.h"
#include "Utility.h"
#include "templates.h"
#include "MultiData.h"

#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>

template <typename Data>
class CacheManager : public CBase_CacheManager<Data> {
public:
  std::mutex local_tps_lock;
  Node<Data>* root;
  std::unordered_map<Key, Node<Data>*> local_tps;
  std::set<Key> prefetch_set;
  std::vector<std::vector<Node<Data>*>> delete_at_end;
  CProxy_Resumer<Data> r_proxy;
  Data nodewide_data;

  CacheManager() {
    initialize();
  }

  void initialize() {
    root = new Node<Data>(1, 0, 0, nullptr, 0, 0, nullptr);
    root->type = Node<Data>::Boundary;
    delete_at_end.resize(CkNodeSize(0));
  }

  ~CacheManager() {
    destroy(false);
  }

  void destroy(bool restore) {
    local_tps.clear();
    prefetch_set.clear();

    for (auto& dae : delete_at_end) {
      for (auto to_delete : dae) {
        delete to_delete;
      }
      dae.resize(0);
    }

    if (root != nullptr) {
      root->triggerFree();
      delete root;
      root = nullptr;
    }

    if (restore) initialize();
  }

  template <typename Visitor>
  void startPrefetch(DPHolder<Data>, CkCallback);
  void startParentPrefetch(DPHolder<Data>, CkCallback);
  void prepPrefetch(Node<Data>*);
  void connect(Node<Data>*, bool);
  void requestNodes(std::pair<Key, int>);
  void serviceRequest(Node<Data>*, int);
  void recvStarterPack(std::pair<Key, Data>* pack, int n, CkCallback);
  void addCache(MultiMsg<Data>*);
  void addCache(MultiData<Data>);
  Node<Data>* addCacheHelper(Particle*, int, Node<Data>*, int);
  void restoreData(std::pair<Key, Data>);
  void restoreDataHelper(std::pair<Key, Data>&, bool);
  void insertNode(Node<Data>*, bool, bool);
  void swapIn(Node<Data>*);
  void process(Key);
};

template <typename Data>
template <typename Visitor>
void CacheManager<Data>::startPrefetch(DPHolder<Data> dp_holder, CkCallback cb) {
  dp_holder.proxy.template prefetch<Visitor>(nodewide_data, this->thisIndex, cb);
}

template <typename Data>
void CacheManager<Data>::startParentPrefetch(DPHolder<Data> dp_holder, CkCallback cb) {
  std::set<Key> request_list;
  for (Key k : prefetch_set) {
    for (int i = 0; i < BRANCH_FACTOR; i++) {
      request_list.insert(k * BRANCH_FACTOR + i);
    }
  }
  request_list.insert(1);
  std::vector<Key> flat_rl (request_list.size());
  std::copy(request_list.begin(), request_list.end(), flat_rl.begin());
  dp_holder.proxy.request(flat_rl.data(), flat_rl.size(), this->thisIndex, cb);
}

template <typename Data>
void CacheManager<Data>::prepPrefetch(Node<Data>* node) {
  // THIS IS IN LOCK
  nodewide_data += node->data;
  Key curr_key = node->key;
  while (curr_key > 1) {
    curr_key /= BRANCH_FACTOR;
    prefetch_set.insert(curr_key);
  }
}

// Invoked by TreePieces after the tree is built
template <typename Data>
void CacheManager<Data>::connect(Node<Data>* node, bool should_process) {
#if DEBUG
  CkPrintf("connecting node %d of type %d\n", node->key, node->type);
#endif
  if (node->type == Node<Data>::CachedBoundary) {
    //node->parent = (node->key == 1) ? nullptr : root->findNode(node->key / BRANCH_FACTOR);
    swapIn(node);
    if (should_process) process(node->key);
  } else {
    if (this->isNodeGroup()) local_tps_lock.lock();

    // Store/connect the incoming TreePiece
    local_tps.insert(std::make_pair(node->key, node));
    prepPrefetch(node);

    if (this->isNodeGroup()) local_tps_lock.unlock();

    if (node->type == Node<Data>::CachedBoundary) {
      CkAbort("local_tps used for a non TP node!");
    }
  }

  // XXX: May need to call process() for dual tree walk
}

template <typename Data>
void CacheManager<Data>::recvStarterPack(std::pair<Key, Data>* pack, int n, CkCallback cb) {
  CkPrintf("[CacheManager %d] receiving starter pack, size = %d\n", this->thisIndex, n);
  for (int i = 0; i < n; i++) {
#if DEBUG
    CkPrintf("[CM %d] receiving node %d in starter pack\n", this->thisIndex, pack[i].first);
#endif
    if (!local_tps.count(pack[i].first)) {
      restoreDataHelper(pack[i], false);
    }
  }
  this->contribute(cb);
}

template <typename Data>
void CacheManager<Data>::addCache(MultiMsg<Data>* multimsg) {
  Node<Data>* top_node = addCacheHelper(multimsg->particles, multimsg->n_particles, multimsg->nodes, multimsg->n_nodes);
  delete multimsg;
  process(top_node->key);
}

template <typename Data>
void CacheManager<Data>::addCache(MultiData<Data> multidata) {
#if DEBUG
  CkPrintf("adding cache for node %d\n", multidata.nodes[0].key);
#endif
  Node<Data>* top_node = addCacheHelper(multidata.particles, multidata.n_particles, multidata.nodes, multidata.n_nodes);
  process(top_node->key);
}

template <typename Data>
Node<Data>* CacheManager<Data>::addCacheHelper(Particle* particles, int n_particles, Node<Data>* nodes, int n_nodes) {
  Node<Data>* first_node_placeholder = r_proxy.ckLocalBranch()->fastNodeFind(nodes[0].key, true);
#if DEBUG
  CkPrintf("adding cache for node %d on cm %d\n", nodes[0].key, this->thisIndex);
#endif
  Node<Data>* first_node;
  if (first_node_placeholder->type != Node<Data>::CachedRemote && first_node_placeholder->type != Node<Data>::CachedRemoteLeaf) {
    int p_index = 0;
    for (int j = 0; j < n_nodes; j++) {
      Node<Data>* node = new Node<Data>(nodes[j]);
      if (j == 0) {
        node->parent = (first_node_placeholder) ? first_node_placeholder->parent : nullptr;
        first_node = node;
      }
      else {
        if (first_node->key == (node->key >> LOG_BRANCH_FACTOR)) node->parent = first_node;
        else node->parent = first_node->children[(node->key >> LOG_BRANCH_FACTOR) % BRANCH_FACTOR].load();
      }
      if (node->type == Node<Data>::Leaf || node->type == Node<Data>::EmptyLeaf) {
        node->type = Node<Data>::CachedRemoteLeaf;
        if (node->n_particles) node->particles = new Particle [node->n_particles];
        for (int i = 0; i < node->n_particles; i++) {
          CkAssert(p_index < n_particles);
          node->particles[i] = particles[p_index++];
        }
      }
      else if (node->type == Node<Data>::Internal) {
        node->type = Node<Data>::CachedRemote;
      }
      insertNode(node, false, j > 0);
    }
  } else CkAbort("Invalid node placeholder type in CacheManager::addCacheHelper");
  swapIn(first_node);
  return first_node;
}

template <typename Data>
void CacheManager<Data>::requestNodes(std::pair<Key, int> param) {
  Key key = param.first;
  Key temp = key;
  while (!local_tps.count(temp)) temp /= BRANCH_FACTOR;
  Node<Data>* node = local_tps[temp]->findNode(key);
  if (!node) {
    CkPrintf("CacheManager::requestNodes: node not found for key %lu on cm %d\n", param.first, this->thisIndex);
    CkAbort("CacheManager::requestNodes: node not found");
  }
  serviceRequest(node, param.second);
}

template <typename Data>
void CacheManager<Data>::serviceRequest(Node<Data>* node, int cm_index) {
  if (cm_index == this->thisIndex) return; // you'll get it later!
  std::vector<Node<Data>> sending_nodes;
  std::vector<Particle> sending_particles;
  sending_nodes.push_back(*node);
  for (int i = 0; i < node->n_children; i++) {
    Node<Data>* child = node->children[i].load();
    sending_nodes.push_back(*child);
    for (int j = 0; j < child->n_children; j++)
      sending_nodes.push_back(*(child->children[j].load()));
  }
  for (auto& to_send : sending_nodes) {
    to_send.cm_index = this->thisIndex;
    if (to_send.type == Node<Data>::Leaf) {
      sending_particles.insert(sending_particles.end(), to_send.particles, to_send.particles + to_send.n_particles);
    }
  }
  MultiData<Data> multidata (sending_particles.data(), sending_particles.size(), sending_nodes.data(), sending_nodes.size());
  this->thisProxy[cm_index].addCache(multidata);
}

template <typename Data>
void CacheManager<Data>::restoreData(std::pair<Key, Data> param) {
  restoreDataHelper(param, true);
}

template <typename Data>
void CacheManager<Data>::restoreDataHelper(std::pair<Key, Data>& param, bool should_process) {
#if DEBUG
  if (!should_process) CkPrintf("restoring data for node %d\n", param.first);
#endif
  Key key = param.first;
  Node<Data>* node = new Node<Data>(key, Node<Data>::CachedBoundary, param.second, BRANCH_FACTOR, (key > 1) ? root->findNode(key >> LOG_BRANCH_FACTOR) : nullptr);
  insertNode(node, true, false);
  connect(node, should_process);
}

template <typename Data>
void CacheManager<Data>::swapIn(Node<Data>* to_swap) {
  if (to_swap->key > 1) {
    to_swap = to_swap->parent->children[to_swap->key % BRANCH_FACTOR].exchange(to_swap);
  }
  else {
    std::swap(root, to_swap);
  }
  delete_at_end[CkMyRank()].push_back(to_swap);
}

template <typename Data>
void CacheManager<Data>::insertNode(Node<Data>* node, bool above_tp, bool should_swap) {
#if DEBUG
  CkPrintf("inserting node %d of type %d with %d children\n", node->key, node->type, node->n_children);
#endif
  for (int i = 0; i < node->n_children; i++) {
    Node<Data>* new_child;
    Key child_key = (node->key << LOG_BRANCH_FACTOR) + i;
    bool add_placeholder = false;
    if (above_tp) {
      auto it = local_tps.find(child_key);
      if (it != local_tps.end()) {
        new_child = it->second;
        new_child->parent = node;
      }
      else {
        add_placeholder = true;
      }
    }
    if (!above_tp || add_placeholder) {
      new_child = new Node<Data> (child_key, node->depth+1, 0, nullptr, 0, 0, node);
      new_child->type = (above_tp) ? Node<Data>::RemoteAboveTPKey : Node<Data>::Remote;
      if (!above_tp) new_child->cm_index = node->cm_index;
    }
    node->children[i].store(new_child);
  }
  if (should_swap) swapIn(node);
}

template <typename Data>
void CacheManager<Data>::process(Key key) {
  if (!this->isNodeGroup()) r_proxy[this->thisIndex].process (key);
  else for (int i = 0; i < CkNodeSize(0); i++) {
    r_proxy[this->thisIndex * CkNodeSize(0) + i].process(key);
  }
}

#endif //PARATREET_CACHEMANAGER_H_
