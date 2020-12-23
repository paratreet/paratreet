#ifndef PARATREET_TRAVERSER_H_
#define PARATREET_TRAVERSER_H_

#include "Subtree.h"
#include "Partition.h"
#include "common.h"
#include "paratreet.decl.h"
#include <stack>
#include <unordered_map>
#include <vector>

namespace {

template <typename Visitor, typename Node, typename StatCollector>
inline bool doOpen(Node* source, Node* target, StatCollector* stats) {
  auto should_open = Visitor::open(*source, *target);
#if COUNT_INTERACTIONS
  stats->countOpen(should_open);
#endif
  return should_open;
}

template <typename Visitor, typename Node, typename StatCollector>
inline void doLeaf(Node* source, Node* target, StatCollector* stats) {
  Visitor::leaf(*source, *target);
#if COUNT_INTERACTIONS
  stats->countLeafInts(target->n_particles);
#endif
}

template <typename Visitor, typename Node, typename StatCollector>
inline void doNode(Node* source, Node* target, StatCollector* stats) {
  Visitor::node(*source, *target);
#if COUNT_INTERACTIONS
  stats->countNodeInts(target->n_particles);
#endif
}

} // empty namespace

template <typename Data>
class Traverser {
public:
  virtual ~Traverser() = default;
  virtual void resumeTrav(Key) = 0;
  virtual void interact() = 0;
  virtual void start() = 0;

protected:
  template <typename Visitor>
  void runSimpleTraversal(Partition<Data>& part, Node<Data>* source_node, int leaf_index)
  {
    std::stack<Node<Data>*> nodes;
    nodes.push(source_node);
    while (!nodes.empty()) {
      Node<Data>* node = nodes.top();
      nodes.pop();
      if (node->type == Node<Data>::Type::Leaf || node->type == Node<Data>::Type::CachedRemoteLeaf) {
        part.interactions[leaf_index].push_back(node);
      } else {
        if (doOpen<Visitor>(node, part.leaves[leaf_index]), part.r_local) {
          for (int j = 0; j < node->n_children; j++) {
            nodes.push(node->getChild(j));
          }
        } else {
          doNode<Visitor>(node, part.leaves[leaf_index], part.r_local);
        }
      }
    }
  }

  template <typename Visitor>
  void interactBase(Partition<Data>& part)
  {
    for (int i = 0; i < part.interactions.size(); i++) {
      for (Node<Data>* source : part.interactions[i]) {
        doLeaf<Visitor>(source, part.leaves[i], part.r_local);
      }
    }
  }
};

template <typename Data, typename Visitor>
class DownTraverser : public Traverser<Data> {
protected:
  std::vector<Node<Data>*> leaves;
  Partition<Data>& part;
  std::unordered_map<Key, std::vector<int>> curr_nodes;
  const bool delay_leaf;

public:
  bool isFinished() {return curr_nodes.empty();}

protected:
  void startTrav(Node<Data>* new_payload) {
    std::vector<int> all_leaves;
    for (int i = 0; i < leaves.size(); i++) all_leaves.push_back(i);
    recurse(new_payload, all_leaves);
  }

public:
  DownTraverser(std::vector<Node<Data>*> leavesi, Partition<Data>& parti, bool delay_leafi = false)
    : leaves(leavesi), part(parti), delay_leaf(delay_leafi)
  { }
  virtual ~DownTraverser() = default;
  virtual void start() override {
    // Initialize with global root key and leaves
    startTrav(part.cm_local->root);
  }
  virtual void interact() override {this->template interactBase<Visitor> (part);}
  void recurse(Node<Data>* node, std::vector<int>& active_buckets) {
    CkAssert(node);
    std::vector<int> new_active_buckets;
    new_active_buckets.reserve(leaves.size());
#if DEBUG
    CkPrintf("tp %d, key = 0x%" PRIx64 ", type = %d, pe %d\n", part.thisIndex, node->key, node->type, CkMyPe());
#endif
    switch (node->type) {
      case Node<Data>::Type::Leaf:
      case Node<Data>::Type::CachedRemoteLeaf:
        {
          // Store local and remote cached leaves for interactions
          for (auto bucket : active_buckets) {
            if (Visitor::CallSelfLeaf || leaves[bucket]->key != node->key) {
              if (delay_leaf) part.interactions[bucket].push_back(node);
              else doLeaf<Visitor>(node, leaves[bucket], part.r_local);
            }
          }
          if (!delay_leaf) node->finish(active_buckets.size());
          break;
        }
      case Node<Data>::Type::Internal:
      case Node<Data>::Type::CachedBoundary:
      case Node<Data>::Type::CachedRemote:
        {
          // Check if the opening condition is fulfilled
          // If so, need to go down deeper
          for (auto bucket : active_buckets) {
            const bool should_open = doOpen<Visitor>(node, leaves[bucket], part.r_local);
            if (should_open) {
              new_active_buckets.push_back(bucket);
            } else {
              // maybe delay as an interaction
              doNode<Visitor>(node, leaves[bucket], part.r_local);
            }
          }
          node->finish(active_buckets.size() - new_active_buckets.size());
          break;
        }
      case Node<Data>::Type::Boundary:
      case Node<Data>::Type::RemoteAboveTPKey:
      case Node<Data>::Type::Remote:
      case Node<Data>::Type::RemoteLeaf:
        {
          curr_nodes[node->key] = active_buckets;

          // Submit a request if the node wasn't requested before
          bool prev = node->requested.exchange(true);
          if (!prev) {
            if (node->type == Node<Data>::Type::Boundary || node->type == Node<Data>::Type::RemoteAboveTPKey) {
              // Ask TreeCanopy for data
              // If the canopy is at the same level as a TP, it asks the TP
              // which eventually calls CacheManager::serviceRequest
              // If the canopy is above TPs, it directly calls
              // CacheManager::restoreData which fills in the cache
              part.tc_proxy[node->key].requestData(part.cm_local->thisIndex);
            }
            else {
              // The node is entirely remote, ask CacheManager for data
              part.cm_proxy[node->cm_index].requestNodes(std::make_pair(node->key, part.cm_local->thisIndex));
            }
          }
          // Add the Partition that initiated the traversal to the waiting list
          // maintained in Resumer
          part.r_local->waiting[node->key].push_back(part.thisIndex);
          break;
        }
      default:
        {
          break;
        }
    }
    if (!new_active_buckets.empty()) {
      for (int idx = 0; idx < node->n_children; idx++) {
        recurse(node->getChild(idx), new_active_buckets);
      }
    }
  }
  virtual void resumeTrav(Key new_key) override {
    auto && resume_nodes = part.r_local->resume_nodes_per_part[part.thisIndex];
    CkAssert(!resume_nodes.empty()); // nothing to resume on?
    while (!resume_nodes.empty()) {
      auto start_node = resume_nodes.front();
      resume_nodes.pop();
      auto key = start_node->key;
#if DEBUG
      CkPrintf("going down on key %d while its type is %d\n", key, start_node->type);
#endif
      auto& now_ready = curr_nodes[key];
      recurse(start_node, now_ready);
      curr_nodes.erase(key);
    }
  }
};

template <typename Data, typename Visitor>
class UpnDTraverser : public DownTraverser<Data, Visitor>
{
private:
  std::vector<Node<Data>*> leaves;
  Node<Data>* trav_top = nullptr;
public:
  UpnDTraverser(Partition<Data>& parti)
    : DownTraverser<Data, Visitor>(parti.leaves, parti, false) {
  }
  virtual ~UpnDTraverser() = default;
  virtual void interact() override {}

  void start() override {
    Key dca_key (1); // deepest common ancestor key
    if (leaves.empty()) return;
    for (auto && leaf : leaves) {
      std::stack<Node<Data>*> nodes;
      nodes.push(leaf);
      trav_top = leaf;
      while (1) {
        while (!nodes.empty()) {
          Node<Data>* node = nodes.top();
          nodes.pop();
          if (node->type == Node<Data>::Type::Internal) {
            if (doOpen<Visitor>(node, leaf, this->part.r_local)) {
              for (int i = 0; i < node->n_children; i++) {
                nodes.push(node->getChild(i));
              }
            }
            else doNode<Visitor>(node, leaf, this->part.r_local);
          }
          else if (node->type == Node<Data>::Type::Leaf) {
            if (Visitor::CallSelfLeaf || node->key != leaf->key) {
              doLeaf<Visitor>(node, leaf, this->part.r_local);
            }
          }
        }
        if (trav_top->key == dca_key) break;
        for (int j = 0; j < trav_top->parent->n_children; j++) {
          auto child = trav_top->parent->getChild(j);
          if (child != trav_top) nodes.push(child);
        }
        trav_top = trav_top->parent;
      }
    }
    // initial traversal just within partition
    // we dont need a permanent state for this phase
    // of the traversal cause we do it all at once
    adjustTop();
  }

  virtual void resumeTrav(Key new_key) override {
    DownTraverser<Data, Visitor>::resumeTrav(new_key);
    adjustTop();
  }

private:
  void adjustTop() {
    if (this->isFinished() && trav_top->parent) {
      for (int j = 0; j < trav_top->parent->n_children; j++) {
        Node<Data>* child = trav_top->parent->getChild(j);
        if (child == nullptr) {
          CkAbort("Error: it's probably that you didn't use startParentPrefetch as your cache loading function\n");
        }
        if (child != trav_top) {
          this->startTrav(child);
        }
      }
      trav_top = trav_top->parent;
      adjustTop();
    }
  }
};
/*
template <typename Data, typename Visitor>
class DualTraverser : public Traverser<Data> {
// NOTE: dual traversals dont have leaves they have payloads
// we start by assigning one payload to each treepiece
private:
  Subtree<Data>& tp;
  std::unordered_map<Key, std::vector<Node<Data>*>> curr_nodes; // source nodes to target nodes
  std::vector<Key> keys;
public:
  DualTraverser(TreePiece<Data>& tpi, std::vector<Key> keysi)
     : tp(tpi), keys(keysi)
  { }
  void start() override {
    tp.global_root = tp.cm_local->root; // these are source nodes
    tp.local_root = tp.global_root->getDescendant(tp.tp_key);
    if (tp.local_root == nullptr) CkAbort("If doing dual traversal, don't set num_share_nodes");
    for (auto key : keys) curr_nodes[key].push_back(tp.local_root);
  }
  virtual void interact() override {this->template interactBase<Visitor>(tp);}

  void runInvertedTraversal(Node<Data>* source_leaf, Node<Data>* target_node)
  {
    std::stack<Node<Data>*> nodes;
    nodes.push(target_node);
    while (!nodes.empty()) {
      Node<Data>* node = nodes.top();
      nodes.pop();
      if (node->type == Node<Data>::Type::Leaf || node->type == Node<Data>::Type::CachedRemoteLeaf) {
        Visitor::leaf(*source_leaf, *node); // why even call leaf() here? maybe just call node.
      } else {
        if (Visitor::open(*node, *source_leaf)) {
          for (int j = 0; j < node->n_children; j++) {
            nodes.push(node->getChild(j));
          }
        } else {
          Visitor::node(*node, *source_leaf);
        }
      }
    }
  }

  virtual void resumeTrav(Key new_key) override {
    auto& now_ready = curr_nodes[new_key];
    std::vector<std::pair<Key, Node<Data>*>> curr_nodes_insertions;
    Node<Data>* start_node = nullptr;
    auto && resume_nodes = tp.r_local->resume_nodes_per_tp[tp.thisIndex];
    if (!resume_nodes.empty()) {
      start_node = resume_nodes.front();
      CkAssert(start_node->key == new_key);
      resume_nodes.pop();
    }
    else {
      start_node = tp.global_root->getDescendant(new_key);
      if (start_node == nullptr) CkAbort("If doing dual traversal, don't set num_share_nodes");
    }
    for (auto new_payload : now_ready) {
      std::stack<std::pair<Node<Data>*, Node<Data>*>> nodes;
      nodes.push(std::make_pair(start_node, new_payload));
      while (nodes.size()) {
        Node<Data>* node = nodes.top().first, *curr_payload = nodes.top().second;
        // node is target, payload is source
        nodes.pop();
#if DEBUG
        CkPrintf("tp %d, key = %d, type = %d, pe %d\n", tp.thisIndex, node->key, node->type, CkMyPe());
#endif
        if (curr_payload->type == Node<Data>::Type::EmptyLeaf) {
          continue;
        }
        else if (curr_payload->type == Node<Data>::Type::Leaf) {
          int leaf_index = -1;
          for (int i = 0; i < tp.leaves.size(); i++) {
            if (tp.leaves[i] == curr_payload) {
              leaf_index = i;
              break;
            }
          }
          CkAssert(leaf_index != -1);
          this->template runSimpleTraversal<Visitor>(tp, node, leaf_index);
          continue;
        }
        CkAssert(curr_payload->type == Node<Data>::Type::Internal);
        switch (node->type) {
          case Node<Data>::Type::Leaf:
          case Node<Data>::Type::CachedRemoteLeaf:
            {
              runInvertedTraversal(node, curr_payload);
              break;
            }
          case Node<Data>::Type::Internal:
          case Node<Data>::Type::CachedBoundary:
          case Node<Data>::Type::CachedRemote:
            {
              if (Visitor::cell(*node, *curr_payload)) {
                for (int i = 0; i < node->n_children; i++) {
                  for (int j = 0; j < curr_payload->n_children; j++) {
                    nodes.push(std::make_pair(node->getChild(i), curr_payload->getChild(j)));
                  }
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
              bool prev = node->requested.exchange(true);
              if (!prev) {
                if (node->type == Node<Data>::Type::Boundary || node->type == Node<Data>::Type::RemoteAboveTPKey)
                  tp.tc_proxy[node->key].requestData(tp.cm_local->thisIndex);
                else tp.cm_proxy[node->cm_index].requestNodes(std::make_pair(node->key, tp.cm_local->thisIndex));
              }
              std::vector<int>& list = tp.r_local->waiting[node->key];
              if (!list.size() || list.back() != tp.thisIndex) list.push_back(tp.thisIndex);
              break;
            }
          default:
            {
              break;
            }
        }
      }
    }
    curr_nodes.erase(new_key);
    for (auto cn : curr_nodes_insertions) curr_nodes[cn.first].push_back(cn.second);
    if (!resume_nodes.empty()) traverse(resume_nodes.front()->key); 
  }
};
*/

#endif // PARATREET_TRAVERSER_H_
