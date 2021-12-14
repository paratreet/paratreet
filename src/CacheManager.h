#ifndef PARATREET_CACHEMANAGER_H_
#define PARATREET_CACHEMANAGER_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Utility.h"
#include "templates.h"
#include "MultiData.h"

#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>

extern CProxy_TreeSpec treespec;

template <typename Data>
struct NodePool {
  virtual ~NodePool() = default;
  virtual Node<Data>* alloc(Key key, int depth, int n_particles, Particle* particles, bool is_leaf, Node<Data>* parent, int tp_index) = 0;
  virtual Node<Data>* alloc(Key key, typename Node<Data>::Type type, SpatialNode<Data> spatial_node, Node<Data>* parent, Particle* particles) = 0;
  virtual void cleanup() = 0;
};

template <typename Data, size_t BranchFactor>
class FullNodePool : public NodePool<Data> {
public:
  FullNodePool(size_t pool_elem_sizei)
  : pool_elem_size(pool_elem_sizei)
  {
    append();
    curr = std::begin(list);
  }
  virtual ~FullNodePool() override {
    for (auto& elem : list) delete[] elem.ptr;
  }
  virtual Node<Data>* alloc(Key key, int depth, int n_particles, Particle* particles, bool is_leaf, Node<Data>* parent, int tp_index) override {
    auto buf = getBuf();
    return new (buf) FullNode<Data, BranchFactor>(key, depth, n_particles, particles, is_leaf, parent, tp_index);
  }
  virtual Node<Data>* alloc(Key key, typename Node<Data>::Type type, SpatialNode<Data> spatial_node, Node<Data>* parent, Particle* particles) override {
    auto buf = getBuf();
    return new (buf) FullNode<Data, BranchFactor>(key, type, spatial_node.is_leaf, spatial_node, particles, parent);
  }
private:
  char* getBuf() {
    lock.lock();
    if (curr->idx == pool_elem_size) {
      if (std::next(curr) == std::end(list)) append();
      curr = std::next(curr);
    }
    auto buf = (FullNode<Data, BranchFactor>*)(curr->ptr) + curr->idx;
    curr->idx++;
    lock.unlock();
    return (char*) buf;
  }
  virtual void cleanup() override {
    for (auto& elem : list) elem.idx = 0;
    curr = std::begin(list);
  }
private:
  void append() {
    list.emplace_back();
    list.back().ptr = new char [sizeof(FullNode<Data, BranchFactor>) * pool_elem_size];
  }
  struct PoolElem {
    char * ptr = nullptr;
    size_t idx = 0;
  };
  const size_t pool_elem_size;
  std::list<PoolElem> list;
  std::mutex lock;
  typename std::list<PoolElem>::iterator curr;
};



template <typename Data>
class CacheManager : public CBase_CacheManager<Data> {
public:
  std::mutex maps_lock;
  Node<Data>* root = nullptr;
  using NodeLookup = std::unordered_map<Key, Node<Data>*>;
  size_t branch_factor = 0;
  NodeLookup local_tps;
  NodeLookup leaf_lookup; // all the cached leaves from copied subtrees
  std::map<Key, std::vector<int>> subtree_copy_started;
  std::map<int, Partition<Data>*> partition_lookup; // managed by Partition
  std::set<Key> prefetch_set;
  std::vector<std::vector<Node<Data>*>> cached_leaves; // cached leaves left over
  std::unique_ptr<NodePool<Data>> pool;
  CProxy_Resumer<Data> r_proxy;
  Data nodewide_data;

  CacheManager() { }

  void initialize(const CkCallback& cb) {
    this->initialize();
    auto& config = paratreet::getConfiguration();
    branch_factor = config.branchFactor();
    auto pool_elem_size = std::max(config.pool_elem_size, 128);
    if (branch_factor == 2) pool.reset(new FullNodePool<Data, 2>(pool_elem_size));
    else if (branch_factor == 8) pool.reset(new FullNodePool<Data, 8>(pool_elem_size));
    else CkAbort("Config branch factor is not 2 or 8. Update list in CacheMananger::initialize to handle this.");
    this->contribute(cb);
  }

  void initialize() {
    cached_leaves.resize(CkNumPes());
  }

  void lockMaps() {
    if (this->isNodeGroup()) maps_lock.lock();
  }
  void unlockMaps() {
    if (this->isNodeGroup()) maps_lock.unlock();
  }

  ~CacheManager() {
    destroy(false);
  }

public:
  void resetCachedParticles(CkCallback cb) {
    for (auto && clv : cached_leaves) {
      for (auto && cl : clv) {
        Data empty_data;
        SpatialNode<Data> empty_sn (empty_data, 0, false, nullptr, 0);
        auto parent = cl->parent;
        auto new_leaf = treespec.ckLocalBranch()->makeCachedNode(cl->key, Node<Data>::Type::Remote, empty_sn, parent, nullptr); // placeholder
        new_leaf->cm_index = cl->cm_index;
        auto which_child = cl->key % cl->getBranchFactor();
        cl->parent->exchangeChild(which_child, new_leaf);
        cl->triggerFree();
      }
      clv.clear();
    }
    this->contribute(cb);
  }
  void destroy(bool restore) {
    pool->cleanup();
    local_tps.clear();
    leaf_lookup.clear();
    subtree_copy_started.clear();
    prefetch_set.clear();
    cached_leaves.clear();

    cleanupPlaceholders();

    root = nullptr;

    if (restore) initialize();
  }

  Node<Data>* makeNode(Key key, int depth, int n_particles, Particle* particles, bool is_leaf, Node<Data>* parent, int tp_index) {
    return pool->alloc(key, depth, n_particles, particles, is_leaf, parent, tp_index);
  }

  Node<Data>* makeCachedNode(Key key, typename Node<Data>::Type type, SpatialNode<Data> spatial_node, Node<Data>* parent, const Particle* particlesToCopy) {
    Particle* particles = nullptr;
    if (spatial_node.is_leaf && spatial_node.n_particles > 0) {
      particles = new Particle [spatial_node.n_particles];
      std::copy(particlesToCopy, particlesToCopy + spatial_node.n_particles, particles);
    }
    return pool->alloc(key, type, spatial_node, parent, particles);
  }

  template <typename Visitor>
  void startPrefetch(DPHolder<Data>, CkCallback);
  void startParentPrefetch(DPHolder<Data>, CkCallback);
  void prepPrefetch(Node<Data>*);
  void requestNodes(std::pair<Key, int>);
  void serviceRequest(Node<Data>*, int);
  void recvStarterPack(std::pair<Key, SpatialNode<Data>>* pack, int n, CkCallback);
  void addCache(MultiData<Data>);
  void receiveSubtree(MultiData<Data>, PPHolder<Data>);
  void restoreData(std::pair<Key, SpatialNode<Data>>);
  void connect(Node<Data>*);

private:
  void makeMsgPerNode(int, std::vector<Node<Data>*>&, std::vector<Particle>&, Node<Data>*);
  Node<Data>* addCacheHelper(Particle*, int, std::pair<Key, SpatialNode<Data>>*, int, int, int, bool);
  void restoreDataHelper(std::pair<Key, SpatialNode<Data>>&, bool);
  void insertNode(Node<Data>*, bool, bool);
  void swapIn(Node<Data>*);
  void process(Key);
  void connect(Node<Data>*, bool);
  void connect(Node<Data>*, const std::vector<Node<Data>*>&);
  void cleanupPlaceholders();
};

template <typename Data>
template <typename Visitor>
void CacheManager<Data>::startPrefetch(DPHolder<Data> dp_holder, CkCallback cb) {
  dp_holder.proxy.template prefetch<Visitor>(nodewide_data, this->thisIndex, cb);
}

template <typename Data>
void CacheManager<Data>::startParentPrefetch(DPHolder<Data> dp_holder, CkCallback cb) {
  std::vector<Key> request_list (prefetch_set.begin(), prefetch_set.end());
  dp_holder.proxy.request(request_list.data(), request_list.size(), this->thisIndex, cb);
}

template <typename Data>
void CacheManager<Data>::prepPrefetch(Node<Data>* node) {
  // THIS IS IN LOCK. make faster? TODO
  nodewide_data += node->data;
  Key curr_key = node->key;
  while (curr_key > 1) {
    curr_key /= branch_factor;
    prefetch_set.insert(curr_key);
    for (int i = 0; i < branch_factor; i++) {
      prefetch_set.insert(curr_key * branch_factor + i);
    }
  }
}

// Invoked to restore a node in the cached tree structure
// or to store the local roots of Subtrees after the tree is built
template <typename Data>
void CacheManager<Data>::connect(Node<Data>* node, bool should_process) {
#if DEBUG
  CkPrintf("connecting node 0x%" PRIx64 " of type %d\n", node->key, node->type);
#endif
  CkAssert(node->type == Node<Data>::Type::CachedBoundary);
  // Invoked internally to update a cached node
  swapIn(node);
  if (should_process) process(node->key);
}

template <typename Data>
void CacheManager<Data>::connect(Node<Data>* node) {
  lockMaps();
  // Store/connect the incoming Subtree's local root
  local_tps.insert(std::make_pair(node->key, node));
  prepPrefetch(node);
  unlockMaps();
  // XXX: May need to call process() for dual tree walk
}

template <typename Data>
void CacheManager<Data>::connect(Node<Data>* node, const std::vector<Node<Data>*>& leaves) {
  lockMaps();
  // Store/connect the incoming Subtree's local root
  local_tps.insert(std::make_pair(node->key, node));
  for (auto && leaf : leaves) leaf_lookup.emplace(leaf->key, leaf);
  unlockMaps();
}

template <typename Data>
void CacheManager<Data>::recvStarterPack(std::pair<Key, SpatialNode<Data>>* pack, int n, CkCallback cb) {
#if !DEBUG
  if (this->thisIndex == 0)
#endif
  CkPrintf("[CacheManager %d] receiving starter pack, size = %d\n", this->thisIndex, n);

  CkAssert(n == 0 || pack[0].first == Key(1));
  for (int i = 0; i < n; i++) {
#if DEBUG
    CkPrintf("[CM %d] receiving node %d in starter pack\n", this->thisIndex, pack[i].first);
#endif
    // uncomment conditional if prefetch() is ever restored
    // if (!local_tps.count(pack[i].first))
    restoreDataHelper(pack[i], false);
  }
  if (n == 0) root = local_tps[1];
  CkAssert(root);
  this->contribute(cb);
}

template <typename Data>
void CacheManager<Data>::receiveSubtree(MultiData<Data> multidata, PPHolder<Data> pp_holder) {
  addCacheHelper(multidata.particles.data(), multidata.particles.size(), multidata.nodes.data(), multidata.nodes.size(), multidata.cm_index, multidata.tp_index, true);
  lockMaps();
  auto copy_out = subtree_copy_started[multidata.tp_index];
  unlockMaps();
  for (auto && partition : copy_out) {
    pp_holder.proxy[partition].makeLeaves(multidata.tp_index);
  }
}

template <typename Data>
void CacheManager<Data>::addCache(MultiData<Data> multidata) {
  Node<Data>* top_node = addCacheHelper(multidata.particles.data(), multidata.particles.size(), multidata.nodes.data(), multidata.nodes.size(), multidata.cm_index, multidata.tp_index, false);
  process(top_node->key);
}

template <typename Data>
Node<Data>* CacheManager<Data>::addCacheHelper(Particle* particles, int n_particles, std::pair<Key, SpatialNode<Data>>* nodes, int n_nodes, int cm_index, int tp_index, bool add_to_tps) {
#if DEBUG
  CkPrintf("adding cache for top node 0x%" PRIx64 " on cm %d\n", nodes[0].first, this->thisIndex);
#endif

  Node<Data>* first_node_placeholder_parent = nullptr;
  if (!add_to_tps) {
    auto first_node_placeholder = (!add_to_tps) ? root->getDescendant(nodes[0].first) : nullptr;
    if (first_node_placeholder->type == Node<Data>::Type::CachedRemote
      || first_node_placeholder->type == Node<Data>::Type::CachedRemoteLeaf)
    {
      CkAbort("Invalid node placeholder type in CacheManager::addCacheHelper");
    }
    first_node_placeholder_parent = first_node_placeholder->parent;
  }

  auto top_type = nodes[0].second.is_leaf ? Node<Data>::Type::CachedRemoteLeaf : Node<Data>::Type::CachedRemote;
  auto first_node = makeCachedNode(nodes[0].first, top_type, nodes[0].second, first_node_placeholder_parent, particles);
  std::vector<Node<Data>*> leaves;
  if (nodes[0].second.is_leaf) leaves.push_back(first_node);
  first_node->cm_index = cm_index;
  first_node->tp_index = tp_index;
  insertNode(first_node, false, false);
  int p_index = nodes[0].second.is_leaf ? nodes[0].second.n_particles : 0;
  for (int j = 1; j < n_nodes; j++) {
    auto && new_key = nodes[j].first;
    auto && spatial_node = nodes[j].second;
    auto curr_parent = first_node->getDescendant(new_key / branch_factor);
    auto type = spatial_node.is_leaf ? Node<Data>::Type::CachedRemoteLeaf : Node<Data>::Type::CachedRemote;
    auto node = makeCachedNode(new_key, type, spatial_node, curr_parent, &particles[p_index]);
    node->cm_index = cm_index;
    node->tp_index = tp_index;
    if (node->is_leaf) {
      p_index += spatial_node.n_particles;
      leaves.push_back(node);
    }
    insertNode(node, false, true);
  }
  if (add_to_tps) connect(first_node, leaves);
  else {
    auto && clv = cached_leaves[CkMyRank()];
    clv.insert(clv.end(), leaves.begin(), leaves.end());
    swapIn(first_node);
  }
  return first_node;
}

template <typename Data>
void CacheManager<Data>::requestNodes(std::pair<Key, int> param) {
  Key key = param.first;
  Key temp = key;
  while (!local_tps.count(temp)) temp /= branch_factor;
  Node<Data>* node = local_tps[temp]->getDescendant(key);
  if (!node) {
    CkPrintf("CacheManager::requestNodes: node not found for key %lu on cm %d\n", param.first, this->thisIndex);
    CkAbort("CacheManager::requestNodes: node not found");
  }
  serviceRequest(node, param.second);
}

template <typename Data>
void CacheManager<Data>::makeMsgPerNode(int start_depth, std::vector<Node<Data>*>& sending_nodes, std::vector<Particle>& sending_particles, Node<Data>* to_process)
{
  auto& config = paratreet::getConfiguration();
  sending_nodes.push_back(to_process);
  if (to_process->type == Node<Data>::Type::Leaf) {
    std::copy(to_process->particles(), to_process->particles() + to_process->n_particles, std::back_inserter(sending_particles));
  }
  if (to_process->depth + 1 < start_depth + config.cache_share_depth) {
    for (int i = 0; i < to_process->n_children; i++) {
      Node<Data>* child = to_process->getChild(i);
      makeMsgPerNode(start_depth, sending_nodes, sending_particles, child);
    }
  }
}

template <typename Data>
void CacheManager<Data>::serviceRequest(Node<Data>* node, int cm_index) {
  if (cm_index == this->thisIndex) return; // you'll get it later!
  std::vector<Node<Data>*> sending_nodes;
  std::vector<Particle> sending_particles;
  makeMsgPerNode(node->depth, sending_nodes, sending_particles, node);
  MultiData<Data> multidata (sending_particles.data(), sending_particles.size(), sending_nodes.data(), sending_nodes.size(), this->thisIndex, node->tp_index);
  this->thisProxy[cm_index].addCache(multidata);
}

template <typename Data>
void CacheManager<Data>::restoreData(std::pair<Key, SpatialNode<Data>> param) {
  restoreDataHelper(param, true);
}

template <typename Data>
void CacheManager<Data>::restoreDataHelper(std::pair<Key, SpatialNode<Data>>& param, bool should_process) {
#if DEBUG
  if (!should_process) CkPrintf("restoring data for node %d\n", param.first);
#endif
  Key key = param.first;
  Node<Data>* parent = (key == Key(1)) ? nullptr : root->getDescendant(key / branch_factor);
  auto node = makeCachedNode(key, Node<Data>::Type::CachedBoundary, param.second, parent, nullptr);
  insertNode(node, true, false);
  connect(node, should_process);
}

template <typename Data>
void CacheManager<Data>::swapIn(Node<Data>* to_swap) {
  if (to_swap->key > 1) {
    auto which_child = to_swap->key % branch_factor;
    to_swap = to_swap->parent->exchangeChild(which_child, to_swap);
  }
  else {
    std::swap(root, to_swap);
  }
}

template <typename Data>
void CacheManager<Data>::insertNode(Node<Data>* node, bool above_tp, bool should_swap) {
#if DEBUG
  CkPrintf("inserting node %d of type %d with %d children\n", node->key, node->type, node->n_children);
#endif
  for (int i = 0; i < node->n_children; i++) {
    Node<Data>* new_child = nullptr;
    Key child_key = node->key * branch_factor + i;
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
      auto type = (above_tp) ? Node<Data>::Type::RemoteAboveTPKey : Node<Data>::Type::Remote;
      Data empty_data;
      SpatialNode<Data> empty_sn (empty_data, 0, false, nullptr, 0);
      new_child = makeCachedNode(child_key, type, empty_sn, node, nullptr); // placeholder
      if (!above_tp) new_child->cm_index = node->cm_index;
    }
    node->exchangeChild(i, new_child);
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

template <typename Data>
void CacheManager<Data>::cleanupPlaceholders() {
  for (auto clv : cached_leaves) {
    for (auto l : clv) l->freeParticles();
    clv.clear();
  }
}


#endif //PARATREET_CACHEMANAGER_H_
