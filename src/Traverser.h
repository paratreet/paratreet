#ifndef PARATREET_TRAVERSER_H_
#define PARATREET_TRAVERSER_H_

#include "TreePiece.h"
#include "common.h"
#include "paratreet.decl.h"
#include <stack>
#include <unordered_map>
#include <vector>

template <typename Data>
class Traverser {
public:
  virtual void traverse(Key) = 0;
  virtual void processLocal() = 0;
  virtual void interact() = 0;
  template <typename Visitor>
  void runSimpleTraversal(TreePiece<Data>* tp, Node<Data>* source_node, int leaf_index)
  {
    Visitor v;
    std::stack<Node<Data>*> nodes;
    nodes.push(source_node);
    while (!nodes.empty()) {
      Node<Data>* node = nodes.top();
      nodes.pop();
      if (node->type == Node<Data>::Type::Leaf || node->type == Node<Data>::Type::CachedRemoteLeaf) {
        tp->interactions[leaf_index].push_back(node);
      }
      else {
        if (v.node(*node, *(tp->leaves[leaf_index]))) {
          for (int j = 0; j < node->n_children; j++) {
            nodes.push(node->getChild(j));
          }
        }
      }
    }
  }

  template <typename Visitor>
  void processLocalBase(TreePiece<Data>* tp)
  {
    for (auto local_trav : tp->local_travs) {
      runSimpleTraversal<Visitor>(tp, local_trav.first, local_trav.second);
    }
  }
  template <typename Visitor>
  void interactBase(TreePiece<Data>* tp)
  {
    Visitor v;
    for (int i = 0; i < tp->interactions.size(); i++) {
      for (Node<Data>* source : tp->interactions[i]) {
        if (source->key != tp->leaves[i]->key) v.leaf(*source, *(tp->leaves[i]));
      }
    }
  }
};

template <typename Data, typename Visitor>
class DownTraverser : public Traverser<Data> {
private:
  TreePiece<Data>* tp;
  std::unordered_map<Key, std::vector<int>> curr_nodes;

public:
  DownTraverser(TreePiece<Data>* tpi) : tp(tpi)
  {
    tp->global_root = tp->cm_local->root;
    // Initialize with global root key and leaves
    for (int i = 0; i < tp->leaves.size(); i++) curr_nodes[1].push_back(i);
  }
  void processLocal() {this->template processLocalBase<Visitor> (tp);}
  void interact() {this->template interactBase<Visitor> (tp);}
  virtual void traverse(Key new_key) {
    Visitor v;
    auto& now_ready = curr_nodes[new_key];
    std::vector<std::pair<Key, int>> curr_nodes_insertions;
    auto start_node = tp->global_root;
    auto && resume_nodes = tp->r_local->resume_nodes_per_tp[tp->thisIndex];
    CkAssert(!resume_nodes.empty() || new_key == 1); // nothing to resume on?
    if (!resume_nodes.empty()) {
      start_node = resume_nodes.front();
      resume_nodes.pop();
    }
#if DEBUG
    CkPrintf("going down on key %d while its type is %d\n", new_key, start_node->type);
#endif
    for (auto bucket : now_ready) {
      std::stack<Node<Data>*, std::vector<Node<Data>*>> nodes;
      nodes.push(start_node);
      while (nodes.size()) {
        Node<Data>* node = nodes.top();
        nodes.pop();
#if DEBUG
        CkPrintf("tp %d, key = %d, type = %d, pe %d\n", tp->thisIndex, node->key, node->type, CkMyPe());
#endif
        switch (node->type) {
          case Node<Data>::Type::Leaf:
          case Node<Data>::Type::CachedRemoteLeaf:
            {
              // Store local and remote cached leaves for interactions
              tp->interactions[bucket].push_back(node);
              break;
            }
          case Node<Data>::Type::Internal:
            {
#if DELAYLOCAL
              tp->local_travs.push_back(std::make_pair(node, bucket));
              break;
#endif
            }
          case Node<Data>::Type::CachedBoundary:
          case Node<Data>::Type::CachedRemote:
            {
              // Check if the 'node' condition is fulfilled
              // If so, need to go down deeper
              if (v.node(*node, *tp->leaves[bucket])) {
                for (int i = 0; i < node->n_children; i++) {
                  nodes.push(node->getChild(i));
                }
              }
              break;
            }
          case Node<Data>::Type::Boundary:
          case Node<Data>::Type::RemoteAboveTPKey:
          case Node<Data>::Type::Remote:
          case Node<Data>::Type::RemoteLeaf:
            {
              curr_nodes_insertions.push_back(std::make_pair(node->key, bucket));

              // Submit a request if the node wasn't requested before
              bool prev = node->requested.exchange(true);
              if (!prev) {
                if (node->type == Node<Data>::Type::Boundary || node->type == Node<Data>::Type::RemoteAboveTPKey) {
                  // Ask TreeCanopy for data
                  // If the canopy is at the same level as a TP, it asks the TP
                  // which eventually calls CacheManager::serviceRequest
                  // If the canopy is above TPs, it directly calls
                  // CacheManager::restoreData which fills in the cache
                  tp->tc_proxy[node->key].requestData(tp->cm_local->thisIndex);
                }
                else {
                  // The node is entirely remote, ask CacheManager for data
                  tp->cm_proxy[node->cm_index].requestNodes(std::make_pair(node->key, tp->cm_local->thisIndex));
                }
              }


              // Add the TreePiece that initiated the traversal to the waiting list
              // maintained in Resumer
              // XXX: Why does it only check the list size and the back of the list?
              std::vector<int>& list = tp->r_local->waiting[node->key];
              if (!list.size() || list.back() != tp->thisIndex) list.push_back(tp->thisIndex);
              break;
            }
          default:
            {
              break;
            }
        }
      }
    }

    // Erase traversed node
    curr_nodes.erase(new_key);

    // Store nodes to be traversed
    for (auto cn : curr_nodes_insertions) curr_nodes[cn.first].push_back(cn.second);
  }
};

template <typename Data, typename Visitor>
class UpnDTraverser : public Traverser<Data> {
private:
  TreePiece<Data>* tp;
  std::unordered_map<Key, std::vector<int>> curr_nodes;
  std::vector<int> num_waiting;
  std::vector<Node<Data>*> trav_tops;
public:
  UpnDTraverser(TreePiece<Data>* tpi) : tp(tpi) {
    trav_tops.resize(tp->leaves.size());
    for (int i = 0; i < tp->leaves.size(); i++) {
      curr_nodes[tp->leaves[i]->key].push_back(i);
      trav_tops[i] = tp->leaves[i];
    }
    num_waiting = std::vector<int> (tp->leaves.size(), 1);
  }
  void processLocal() {CkPrintf("no need to process local traversals\n");}
  void interact() {CkPrintf("no need to perform interactions\n");}

  virtual void traverse(Key new_key) {
    Visitor v;
    auto& now_ready = curr_nodes[new_key];
    std::vector<std::pair<Key, int>> curr_nodes_insertions;
    auto && resume_nodes = tp->r_local->resume_nodes_per_tp[tp->thisIndex];
    Node<Data>* resume_node = nullptr;
    if (!resume_nodes.empty()) {
      resume_node = resume_nodes.front();
      resume_nodes.pop();
    }
#if DEBUG
    CkPrintf("going down on key %d while its type is %d, pe is %d\n", new_key, start_node->type, CkMyPe());
#endif
    for (auto bucket : now_ready) {
      num_waiting[bucket]--;
      auto start_node = resume_node ? resume_node : trav_tops[bucket];
      std::stack<Node<Data>*> nodes;
      nodes.push(start_node);
      while (nodes.size()) {
        Node<Data>* node = nodes.top();
        nodes.pop();
#if DEBUG
        CkPrintf("tp %d, key = %d, type = %d, pe %d\n", tp->thisIndex, node->key, node->type, CkMyPe());
#endif
        switch (node->type) {
          case Node<Data>::Type::Leaf:
          case Node<Data>::Type::CachedRemoteLeaf:
            {
              v.leaf(*node, *(tp->leaves[bucket]));
              break;
            }
          case Node<Data>::Type::Internal:
          case Node<Data>::Type::CachedBoundary:
          case Node<Data>::Type::CachedRemote:
            {
              if (v.node(*node, *(tp->leaves[bucket]))) {
                for (int i = 0; i < node->n_children; i++) {
                  nodes.push(node->getChild(i));
                }
              }
              break;
            }
          case Node<Data>::Type::Boundary:
          case Node<Data>::Type::RemoteAboveTPKey:
          case Node<Data>::Type::Remote:
          case Node<Data>::Type::RemoteLeaf:
            {
              curr_nodes_insertions.push_back(std::make_pair(node->key, bucket));
              num_waiting[bucket]++;
              bool prev = node->requested.exchange(true);
              if (!prev) {
                if (node->type == Node<Data>::Type::Boundary || node->type == Node<Data>::Type::RemoteAboveTPKey)
                  tp->tc_proxy[node->key].requestData(tp->cm_local->thisIndex);
                else tp->cm_proxy[node->cm_index].requestNodes(std::make_pair(node->key, tp->cm_local->thisIndex));
              }
              std::vector<int>& list = tp->r_local->waiting[node->key];
              if (!list.size() || list.back() != tp->thisIndex) list.push_back(tp->thisIndex);
              break;
            }
          default:
            {
              break;
            }
        }
      }
      if (num_waiting[bucket] == 0) {
        if (trav_tops[bucket]->parent) {
          for (int j = 0; j < trav_tops[bucket]->n_children; j++) {
            Node<Data>* child = trav_tops[bucket]->parent->getChild(j);
            if (child == nullptr) {
              CkPrintf("child of key %lu and parent type %d is nullptr\n", trav_tops[bucket]->parent->key * 8 + j, trav_tops[bucket]->parent->type);
            }
            if (child != trav_tops[bucket]) {
               if (trav_tops[bucket]->parent->type == Node<Data>::Type::Boundary) {
                 child->type = Node<Data>::Type::RemoteAboveTPKey;
               }
               curr_nodes_insertions.push_back(std::make_pair(child->key, bucket));
               num_waiting[bucket]++;
               tp->thisProxy[tp->thisIndex].goDown(child->key);
            }
          }
          trav_tops[bucket] = trav_tops[bucket]->parent;
        }
      }
    }
    //else CkPrintf("tp %d leaf %d still waiting\n", tp->thisIndex, i);
    curr_nodes.erase(new_key);
    for (auto cn : curr_nodes_insertions) curr_nodes[cn.first].push_back(cn.second);
  }
};

template <typename Data, typename Visitor>
class DualTraverser : public Traverser<Data> {
// NOTE: dual traversals dont have leaves they have payloads
// we start by assigning one payload to each treepiece
private:
  TreePiece<Data>* tp;
  std::unordered_map<Key, std::vector<Node<Data>*>> curr_nodes; // source nodes to target nodes
public:
  DualTraverser(TreePiece<Data>* tpi, std::vector<Key> keys) : tp(tpi)
  {
    tp->global_root = tp->cm_local->root; // these are source nodes
    tp->local_root = tp->global_root->getDescendant(tp->tp_key);
    if (tp->local_root == nullptr) CkAbort("If doing dual traversal you need to set num_share_levels = 0");
    for (auto key : keys) curr_nodes[key].push_back(tp->local_root);
  }
  void processLocal () {}
  void interact() {this->template interactBase<Visitor>(tp);}

  void runInvertedTraversal(Node<Data>* source_leaf, Node<Data>* target_node)
  {
    Visitor v;
    std::stack<Node<Data>*> nodes;
    nodes.push(target_node);
    while (!nodes.empty()) {
      Node<Data>* node = nodes.top();
      nodes.pop();
      if (node->type == Node<Data>::Type::Leaf || node->type == Node<Data>::Type::CachedRemoteLeaf) {
        v.leaf(*source_leaf, *node);
      }
      else {
        if (v.node(*node, *source_leaf)) {
          for (int j = 0; j < node->n_children; j++) {
            nodes.push(node->getChild(j));
          }
        }
      }
    }
  }

  virtual void traverse(Key new_key) {
    Visitor v;
    auto& now_ready = curr_nodes[new_key];
    std::vector<std::pair<Key, Node<Data>*>> curr_nodes_insertions;
    Node<Data>* start_node = nullptr;
    auto && resume_nodes = tp->r_local->resume_nodes_per_tp[tp->thisIndex];
    if (!resume_nodes.empty()) {
      start_node = resume_nodes.front();
      CkAssert(start_node->key == new_key);
      resume_nodes.pop();
    }
    else {
      start_node = tp->global_root->getDescendant(new_key);
      if (start_node == nullptr) CkAbort("If doing dual traversal you need to set num_share_levels = 0");
    }
    for (auto new_payload : now_ready) {
      std::stack<std::pair<Node<Data>*, Node<Data>*>> nodes;
      nodes.push(std::make_pair(start_node, new_payload));
      while (nodes.size()) {
        Node<Data>* node = nodes.top().first, *curr_payload = nodes.top().second;
        // node is target, payload is source
        nodes.pop();
#if DEBUG
        CkPrintf("tp %d, key = %d, type = %d, pe %d\n", tp->thisIndex, node->key, node->type, CkMyPe());
#endif
        if (curr_payload->type == Node<Data>::Type::EmptyLeaf) {
          continue;
        }
        else if (curr_payload->type == Node<Data>::Type::Leaf) {
          int leaf_index = -1;
          for (int i = 0; i < tp->leaves.size(); i++) {
            if (tp->leaves[i] == curr_payload) {
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
              if (v.cell(*node, *curr_payload)) {
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
                  tp->tc_proxy[node->key].requestData(tp->cm_local->thisIndex);
                else tp->cm_proxy[node->cm_index].requestNodes(std::make_pair(node->key, tp->cm_local->thisIndex));
              }
              std::vector<int>& list = tp->r_local->waiting[node->key];
              if (!list.size() || list.back() != tp->thisIndex) list.push_back(tp->thisIndex);
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
  }
};

#endif // PARATREET_TRAVERSER_H_
