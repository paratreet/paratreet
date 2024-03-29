#ifndef PARATREET_TRAVERSER_H_
#define PARATREET_TRAVERSER_H_

#include "common.h"
#include "paratreet.decl.h"
#include <stack>
#include <deque>
#include <unordered_map>
#include <vector>
#include <bitset>

namespace {

template <typename Visitor, typename Node, typename StatCollector>
inline bool doOpen(Visitor& v, Node* source, Node* target, StatCollector* stats) {
  auto should_open = v.open(*source, *target);
#if COUNT_INTERACTIONS
  stats->countOpen(should_open);
#endif
  return should_open;
}

template <typename Visitor, typename Node, typename StatCollector>
inline void doLeaf(Visitor& v, Node* source, Node* target, StatCollector* stats) {
  v.leaf(*source, *target);
#if COUNT_INTERACTIONS
  stats->countLeafInts(source->n_particles * target->n_particles);
#endif
}

template <typename Visitor, typename Node, typename StatCollector>
inline void doNode(Visitor& v, Node* source, Node* target, StatCollector* stats) {
  v.node(*source, *target);
#if COUNT_INTERACTIONS
  stats->countNodeInts(target->n_particles);
#endif
}

template <typename Visitor, typename Node, typename StatCollector>
inline bool doCell(Visitor& v, Node* source, Node* target, StatCollector* stats) {
  auto should_open = v.cell(*source, *target);
#if COUNT_INTERACTIONS
  stats->countOpen(should_open);
#endif
  return should_open;
}

template <typename Data>
// returns if_requested
inline bool handleRemoteNode(Node<Data>* node, size_t trav_idx, size_t part_idx, CProxy_TreeCanopy<Data> tc_proxy, CProxy_CacheManager<Data> cm_proxy, CProxy_Resumer<Data> r_proxy) {
  auto cm_index = cm_proxy.ckLocalBranch()->thisIndex;
  auto mask = 1ull << CkMyRank();
  auto prev = node->requested.fetch_or(mask);
  bool changed = prev | mask == 0;
  if (prev == 0) {
    if (node->type == Node<Data>::Type::Boundary || node->type == Node<Data>::Type::RemoteAboveTPKey) {
      // Ask TreeCanopy for data
      // If the canopy is at the same level as a TP, it asks the TP
      // which eventually calls CacheManager::serviceRequest
      // If the canopy is above TPs, it directly calls
      // CacheManager::restoreData which fills in the cache
      tc_proxy[node->key].requestData(cm_index);
    }
    else {
      // The node is entirely remote, ask CacheManager for data
      cm_proxy[node->cm_index].requestNodes(std::make_pair(node->key, cm_index));
    }
  // it is possible that the node has been substituted already. in this case, check if the placeholder is still the same
  } else if (changed && node->parent && node->parent->getDescendant(node->key) != node) r_proxy.process(node->key);
  // Add the Partition that initiated the traversal to the waiting list
  // maintained in Resumer
  auto& list = r_proxy.ckLocalBranch()->waiting[node->key];
  if (list.empty() || list.back().first != trav_idx || list.back().second != part_idx) list.emplace_back(trav_idx, part_idx);
  return prev == 0;
}

} // empty namespace

template <typename Data>
class Traverser {
public:
  virtual ~Traverser() = default;
  virtual void resumeTrav() = 0;
  virtual void interact() = 0;
  virtual void start() = 0;
  virtual bool isFinished() = 0;
  virtual bool wantsPause() const {return false;}
  virtual void resumeAfterPause() {}
};

template <typename Data, typename Visitor>
class TransposedDownTraverser : public Traverser<Data> {
public:
  using ABType = std::vector<bool>;

protected:
  Visitor v;
  size_t trav_idx = 0;
  std::vector<Node<Data>*> leaves;
  Partition<Data>& part;
  ThreadStateHolder* stats = nullptr;
  std::unordered_map<Key, ABType> curr_nodes;
  std::vector<std::pair<Node<Data>*, ABType>> paused_curr_nodes;
  int num_requested = 0;
  int handle_count = 0;
  int request_pause_interval = 0;
  int iter_pause_interval = 0;
  std::vector<std::vector<Node<Data>*>> interactions;
  const bool delay_leaf;

protected:
  void startTrav(Node<Data>* new_payload) {
    ABType all_leaves (leaves.size(), true);
    recurse(new_payload, all_leaves);
  }

public:
  TransposedDownTraverser(Visitor& vi, size_t ti, std::vector<Node<Data>*> leavesi, Partition<Data>& parti, bool delay_leafi = false)
    : v(vi), trav_idx(ti), leaves(leavesi), part(parti), delay_leaf(delay_leafi)
  {
    request_pause_interval = paratreet::getConfiguration().request_pause_interval;
    iter_pause_interval = paratreet::getConfiguration().iter_pause_interval;
    stats = thread_state_holder.ckLocalBranch();
    if (delay_leaf) interactions.resize(leaves.size());
  }
  virtual ~TransposedDownTraverser() = default;
  virtual bool isFinished() override {return curr_nodes.empty();}
  virtual void start() override {
    // Initialize with global root key and leaves
    startTrav(part.cm_local->root);
  }
  virtual void interact() override {
    for (int i = 0; i < interactions.size(); i++) {
      for (Node<Data>* source : interactions[i]) {
        doLeaf(v, source, part.leaves[i], stats);
      }
    }
  }
  virtual bool wantsPause() const override {return handle_count > iter_pause_interval || num_requested > request_pause_interval;}
  virtual void resumeAfterPause() override{
    num_requested = 0;
    handle_count = 0;
    size_t idx = 0u;
    for (; idx < paused_curr_nodes.size(); idx++) {
      if (wantsPause()) break;
      auto paused = paused_curr_nodes[idx];
      for (size_t childIdx = 0; childIdx < paused.first->n_children; childIdx++) {
        recurse(paused.first->getChild(childIdx), paused.second);
      }
    }
    paused_curr_nodes.erase(paused_curr_nodes.begin(), paused_curr_nodes.begin() + idx); // erase all up until idx
  }

  void recurse(Node<Data>* node, const ABType& active_buckets) {
    CkAssert(node);
    ABType new_active_buckets (leaves.size(), false);
    bool continue_trav = false;
#if DEBUG
    CkPrintf("tp %d, key = 0x%" PRIx64 ", type = %d, pe %d\n", part.thisIndex, node->key, (int)node->type, CkMyPe());
#endif
    switch (node->type) {
      case Node<Data>::Type::Leaf:
      case Node<Data>::Type::CachedRemoteLeaf:
        {
          // Store local and remote cached leaves for interactions
          for (int bucket = 0; bucket < leaves.size(); bucket++) {
            if (active_buckets[bucket] && (Visitor::CallSelfLeaf || leaves[bucket]->key != node->key)) {
              if (delay_leaf) interactions[bucket].push_back(node);
              else doLeaf(v, node, leaves[bucket], stats);
            }
          }
          break;
        }
      case Node<Data>::Type::Internal:
      case Node<Data>::Type::CachedBoundary:
      case Node<Data>::Type::CachedRemote:
        {
          // Check if the opening condition is fulfilled
          // If so, need to go down deeper
          for (int bucket = 0; bucket < leaves.size(); bucket++) {
            if (!active_buckets[bucket]) continue;
            const bool should_open = doOpen(v, node, leaves[bucket], stats);
            new_active_buckets[bucket] = should_open;
            if (should_open) {
              continue_trav = true;
            } else {
              // maybe delay as an interaction
              doNode(v, node, leaves[bucket], stats);
            }
          }
          break;
        }
      case Node<Data>::Type::Boundary:
      case Node<Data>::Type::RemoteAboveTPKey:
      case Node<Data>::Type::Remote:
      case Node<Data>::Type::RemoteLeaf:
        {
          curr_nodes[node->key] = active_buckets;
          if (handleRemoteNode(node, trav_idx, part.thisIndex, part.tc_proxy, part.cm_proxy, part.r_proxy)) num_requested++;
          break;
        }
      default:
        {
          break;
        }
    }
    if (continue_trav) {
      handle_count++;
      if (node->type == Node<Data>::Type::Internal && wantsPause()) {
        paused_curr_nodes.emplace_back(node, new_active_buckets);
      } else {
        for (int idx = 0; idx < node->n_children; idx++) {
          recurse(node->getChild(idx), new_active_buckets);
        }
      }
    }
  }
  virtual void resumeTrav() override {
    auto && resume_nodes = part.r_local->all_resume_nodes[std::make_pair(trav_idx, part.thisIndex)];
    CkAssert(!resume_nodes.empty()); // nothing to resume on?
    while (!resume_nodes.empty()) {
      auto start_node = resume_nodes.front();
      resume_nodes.pop();
      auto key = start_node->key;
#if DEBUG
      CkPrintf("going down on key %d while its type is %d\n", key, (int)start_node->type);
#endif
      auto now_ready = curr_nodes[key];
      recurse(start_node, now_ready);
      curr_nodes.erase(key);
    }
  }
};

template <typename Data, typename Visitor>
class BasicDownTraverser : public Traverser<Data> {
protected:
  Visitor v;
  size_t trav_idx;
  std::vector<Node<Data>*> leaves;
  Partition<Data>& part;
  ThreadStateHolder* stats = nullptr;
  std::unordered_map<Key, std::vector<int>> curr_nodes;
  int num_requested = 0;
  int saved_start_idx = 0;
  int request_pause_interval = 0;
  int next_stop_index = 0;
  std::vector<std::vector<Node<Data>*>> interactions;
  const bool delay_leaf;

protected:
  void startTrav() {
    for (; saved_start_idx < leaves.size(); saved_start_idx++) {
      if (wantsPause()) break;
      doTrav(part.cm_local->root, saved_start_idx);
    }
    //if (saved_start_idx < leaves.size()) CkPrintf("quitting ssi at %d\n", saved_start_idx);
  }

public:
  BasicDownTraverser(Visitor& vi, size_t ti, std::vector<Node<Data>*> leavesi, Partition<Data>& parti, bool delay_leafi = false)
    : v(vi), trav_idx(ti), leaves(leavesi), part(parti), delay_leaf(delay_leafi)
  {
    request_pause_interval = paratreet::getConfiguration().request_pause_interval;
    auto iter_pause_interval = paratreet::getConfiguration().iter_pause_interval;
    next_stop_index += iter_pause_interval > 0 ? iter_pause_interval : leaves.size();
    if (delay_leaf) interactions.resize(leaves.size());
    stats = thread_state_holder.ckLocalBranch();
  }
  virtual ~BasicDownTraverser() = default;
  virtual bool isFinished() override {return curr_nodes.empty();}
  virtual void start() override {
    // Initialize with global root key and leaves
    startTrav();
  }
  virtual void interact() override {
    for (int i = 0; i < interactions.size(); i++) {
      for (Node<Data>* source : interactions[i]) {
        doLeaf(v, source, part.leaves[i], stats);
      }
    }
  }
  virtual bool wantsPause() const override {return saved_start_idx > next_stop_index || num_requested > request_pause_interval;}
  virtual void resumeAfterPause() override {
    num_requested = 0;
    auto iter_pause_interval = paratreet::getConfiguration().iter_pause_interval;
    next_stop_index += iter_pause_interval > 0 ? iter_pause_interval : leaves.size();
    startTrav();
  }
  void doTrav(Node<Data>* start_node, int bucket) {
    CkAssert(start_node);
#if DEBUG
    CkPrintf("tp %d, key = 0x%" PRIx64 ", type = %d, pe %d\n", part.thisIndex, start_node->key, (int)start_node->type, CkMyPe());
#endif
    std::deque<Node<Data>*> nodes;
    nodes.push_front(start_node);
    while (!nodes.empty()) {
      auto node = nodes.back(); // maybe make it to do front or back.
      nodes.pop_back(); // same as above
      switch (node->type) {
        case Node<Data>::Type::Leaf:
        case Node<Data>::Type::CachedRemoteLeaf:
          {
            // Store local and remote cached leaves for interactions
            if (Visitor::CallSelfLeaf || leaves[bucket]->key != node->key) {
              if (delay_leaf) interactions[bucket].push_back(node);
              else doLeaf(v, node, leaves[bucket], stats);
            }
            break;
          }
        case Node<Data>::Type::Internal:
        case Node<Data>::Type::CachedBoundary:
        case Node<Data>::Type::CachedRemote:
          {
            // Check if the opening condition is fulfilled
            // If so, need to go down deeper
            const bool should_open = doOpen(v, node, leaves[bucket], stats);
            if (should_open) {
              for (int idx = 0; idx < node->n_children; idx++) {
                nodes.push_front(node->getChild(idx));
              }
            } else {
              // maybe delay as an interaction
              doNode(v, node, leaves[bucket], stats);
            }
            break;
          }
        case Node<Data>::Type::Boundary:
        case Node<Data>::Type::RemoteAboveTPKey:
        case Node<Data>::Type::Remote:
        case Node<Data>::Type::RemoteLeaf:
          {
            curr_nodes[node->key].push_back(bucket);
            if (handleRemoteNode(node, trav_idx, part.thisIndex, part.tc_proxy, part.cm_proxy, part.r_proxy)) num_requested++;
            break;
          }
        default:
          {
            break;
          }
      }
    }
  }
  virtual void resumeTrav() override {
    auto && resume_nodes = part.r_local->all_resume_nodes[std::make_pair(trav_idx, part.thisIndex)];
    CkAssert(!resume_nodes.empty()); // nothing to resume on?
    while (!resume_nodes.empty()) {
      auto start_node = resume_nodes.front();
      resume_nodes.pop(); // TODO fix
      auto key = start_node->key;
#if DEBUG
      CkPrintf("going down on key %d while its type is %d\n", key, (int)start_node->type);
#endif
      auto& now_ready = curr_nodes[key];
      for (auto bucket : now_ready) doTrav(start_node, bucket);
      curr_nodes.erase(key);
    }
  }
};


template <typename Data, typename Visitor>
class UpnDTraverser : public Traverser<Data> {
private:
  Visitor v;
  size_t trav_idx;
  Partition<Data>& part;
  ThreadStateHolder* stats = nullptr;
  std::unordered_map<Key, std::vector<int>> curr_nodes;
  std::vector<int> num_waiting;
  std::vector<Node<Data>*> trav_tops;
public:
  UpnDTraverser(Visitor& vi, size_t ti, Partition<Data>& parti) : v(vi), trav_idx(ti), part(parti) {
    trav_tops.resize(part.leaves.size());
    for (int i = 0; i < part.leaves.size(); i++) {
      auto tree_leaf = part.tree_leaves[i];
      curr_nodes[tree_leaf->key].push_back(i);
      trav_tops[i] = tree_leaf;
    }
    num_waiting = std::vector<int> (part.leaves.size(), 1);
    stats = thread_state_holder.ckLocalBranch();
  }
  virtual void interact() override {}
  virtual bool isFinished() override {return curr_nodes.empty();}
  virtual void start() override {
    for (auto && trav_top : trav_tops) {
      traverse(trav_top);
    }
    resumeTrav();
  }

  virtual void resumeTrav() {
    auto && resume_nodes = part.r_local->all_resume_nodes[std::make_pair(trav_idx, part.thisIndex)];
    while (!resume_nodes.empty()) {
      auto start_node = resume_nodes.front();
      resume_nodes.pop();
      auto key = start_node->key;
#if DEBUG
      CkPrintf("going down on key %d while its type is %d\n", key, (int)start_node->type);
#endif
      traverse(start_node);
    }
  }

private:
  void traverse(Node<Data>* start_node) {
    auto key = start_node->key;
    auto& now_ready = curr_nodes[key];
    std::vector<std::pair<Key, int>> curr_nodes_insertions;
    std::set<Node<Data>*> new_nodes;
    for (auto bucket : now_ready) {
      num_waiting[bucket]--;
      std::stack<Node<Data>*> nodes;
      nodes.push(start_node);
      while (nodes.size()) {
        Node<Data>* node = nodes.top();
        nodes.pop();
#if DEBUG
        CkPrintf("tp %d, key = %d, type = %d, pe %d\n", part.thisIndex, node->key, (int)node->type, CkMyPe());
#endif
        switch (node->type) {
          case Node<Data>::Type::Leaf:
          case Node<Data>::Type::CachedRemoteLeaf:
            {
              doLeaf(v, node, part.leaves[bucket], stats);
              break;
            }
          case Node<Data>::Type::Internal:
          case Node<Data>::Type::CachedBoundary:
          case Node<Data>::Type::CachedRemote:
            {
              if (doOpen(v, node, part.leaves[bucket], stats)) {
                for (int i = 0; i < node->n_children; i++) {
                  nodes.push(node->getChild(i));
                }
              } else {
                doNode(v, node, part.leaves[bucket], stats);
              }
              break;
            }
          case Node<Data>::Type::Boundary:
          case Node<Data>::Type::RemoteAboveTPKey:
          case Node<Data>::Type::Remote:
          case Node<Data>::Type::RemoteLeaf:
            {
              curr_nodes_insertions.push_back(std::make_pair(node->key, bucket));
              handleRemoteNode(node, trav_idx, part.thisIndex, part.tc_proxy, part.cm_proxy, part.r_proxy);
              break;
            }
          default:
            {
              break;
            }
        }
      }
      if (num_waiting[bucket] == 0) {
        auto trav_top_parent = trav_tops[bucket]->parent;
        if (trav_top_parent) {
          for (int j = 0; j < trav_top_parent->n_children; j++) {
            Node<Data>* child = trav_top_parent->getChild(j);
            if (child == nullptr) {
              CkPrintf("child of key %lu and parent type %d is nullptr\n", trav_top_parent->key * 8 + j, (int)trav_top_parent->type);
            }
            if (child != trav_tops[bucket]) {
               //if (trav_top_parent->type == Node<Data>::Type::Boundary) {
               //  child->type = Node<Data>::Type::RemoteAboveTPKey;
               //}
               curr_nodes_insertions.push_back(std::make_pair(child->key, bucket));
               num_waiting[bucket]++;
               new_nodes.insert(child);
            }
          }
          trav_tops[bucket] = trav_tops[bucket]->parent;
        }
      }
    }
    curr_nodes.erase(key);
    for (auto cn : curr_nodes_insertions) curr_nodes[cn.first].push_back(cn.second);
    auto && resume_nodes = part.r_local->all_resume_nodes[std::make_pair(trav_idx, part.thisIndex)];
    for (auto && new_node : new_nodes) resume_nodes.push(new_node);
  }
};

template <typename Data, typename Visitor>
class DualTraverser : public Traverser<Data> {
// NOTE: dual traversals dont have leaves they have payloads
// we start by assigning one payload to each treepiece
private:
  Visitor v;
  size_t trav_idx;
  Subtree<Data>& tp;
  ThreadStateHolder* stats = nullptr;
  std::unordered_map<Key, std::vector<Node<Data>*>> curr_nodes; // source nodes to target nodes
public:
  DualTraverser(Visitor& vi, size_t ti, Subtree<Data>& tpi) : v(vi), trav_idx(ti), tp(tpi)
  {
    stats = thread_state_holder.ckLocalBranch();
  }
  void start() override {
    curr_nodes[1].push_back(tp.local_root);
    doTrav(tp.cm_local->root);
    // do work
  }
  virtual void interact() override {}
  virtual bool isFinished() override {return curr_nodes.empty();}

  void runInvertedTraversal(Node<Data>* source_leaf, Node<Data>* target_node)
  {
    std::stack<Node<Data>*> nodes;
    nodes.push(target_node);
    while (!nodes.empty()) {
      Node<Data>* node = nodes.top();
      nodes.pop();
      if (node->isLeaf()) {
        doLeaf(v, source_leaf, node, stats);
      } else {
        if (doOpen(v, source_leaf, node, stats)) { // should we even check this?
          // if so, wouldnt that justify checking open() before leaf() on leaves?
          for (int j = 0; j < node->n_children; j++) {
            nodes.push(node->getChild(j));
          }
        } else {
          doNode(v, source_leaf, node, stats);
        }
      }
    }
  }

  virtual void resumeTrav() override {
    auto && resume_nodes = tp.r_local->all_resume_nodes[std::make_pair(trav_idx, tp.thisIndex)];
    while (!resume_nodes.empty()) {
      doTrav(resume_nodes.front());
      resume_nodes.pop();
    }
  }

  void doTrav(Node<Data>* start_node) {
    Key new_key = start_node->key;
    std::vector<std::pair<Key, Node<Data>*>> curr_nodes_insertions;
    auto& now_ready = curr_nodes[new_key];
    std::stack<std::pair<Node<Data>*, Node<Data>*>> nodes;
    for (auto && np : now_ready) nodes.emplace(start_node, np);
    while (!nodes.empty()) {
      Node<Data>* node = nodes.top().first, *curr_payload = nodes.top().second;
      // node is source, payload is target
      nodes.pop();
#if DEBUG
      CkPrintf("tp %d, target key = %d, type = %d, source key = %d, type = %d, pe %d\n", tp.thisIndex, node->key, (int)node->type, curr_payload->key, (int)curr_payload->type, CkMyPe());
#endif
      if (curr_payload->type == Node<Data>::Type::EmptyLeaf) {
        continue;
      }
      switch (node->type) {
        case Node<Data>::Type::Leaf:
        case Node<Data>::Type::CachedRemoteLeaf:
          {
            if (curr_payload->isLeaf() || !Visitor::TargetMustBeLeaf) {
              doLeaf(v, node, curr_payload, stats); // n2 calc
            } else runInvertedTraversal(node, curr_payload);
            break;
          }
        case Node<Data>::Type::Internal:
        case Node<Data>::Type::CachedBoundary:
        case Node<Data>::Type::CachedRemote:
          {
            if (curr_payload->isLeaf()) {
              if (doOpen(v, node, curr_payload, stats)) {
                for (int i = 0; i < node->n_children; i++) {
                  nodes.emplace(node->getChild(i), curr_payload);
                }
              }
              else doNode(v, node, curr_payload, stats);
            }
            else if (!doCell(v, node, curr_payload, stats)) {
              // cell means should we open target
              if (Visitor::TargetMustBeLeaf) runInvertedTraversal(node, curr_payload);
              else if (doOpen(v, node, curr_payload, stats)) {
                for (int i = 0; i < node->n_children; i++) {
                  nodes.emplace(node->getChild(i), curr_payload);
                }
              }
              else doNode(v, node, curr_payload, stats);
            }
            else {
              for (int i = 0; i < node->n_children; i++) {
                if (!Visitor::ForceEvenDepth || curr_payload->depth <= node->depth) {
                  for (int j = 0; j < curr_payload->n_children; j++) {
                    nodes.emplace(node->getChild(i), curr_payload->getChild(j));
                  }
                } else nodes.emplace(node->getChild(i), curr_payload);
              }
            }
            break;
          }
        case Node<Data>::Type::Boundary:
        case Node<Data>::Type::RemoteAboveTPKey:
        case Node<Data>::Type::Remote:
        case Node<Data>::Type::RemoteLeaf:
          {
            curr_nodes_insertions.push_back(std::make_pair(node->key, curr_payload));
            handleRemoteNode(node, trav_idx, tp.thisIndex, tp.tc_proxy, tp.cm_proxy, tp.r_proxy);
            break;
          }
        default: break;
      }
    }
    curr_nodes.erase(new_key);
    for (auto cn : curr_nodes_insertions) curr_nodes[cn.first].push_back(cn.second);
  }
};

#endif // PARATREET_TRAVERSER_H_
