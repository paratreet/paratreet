#ifndef SIMPLE_TRAVERSER_H_
#define SIMPLE_TRAVERSER_H_

#include "TreePiece.h"
#include "common.h"
#include "simple.decl.h"
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
  void processLocalBase(TreePiece<Data>* tp)
  {
    Visitor v;
    for (auto local_trav : tp->local_travs) {
      std::stack<Node<Data>*> nodes;
      nodes.push(local_trav.first);
      while (nodes.size()) {
        Node<Data>* node = nodes.top();
        nodes.pop();
        if (node->type == Node<Data>::Internal) {
          if (v.node(node, tp->leaves[local_trav.second])) {
            for (int j = 0; j < node->n_children; j++) {
              nodes.push(node->children[j].load());
            }
          }
        }
        else {
          tp->interactions[local_trav.second].push_back(node);
        }
      }
    }
  }
  template <typename Visitor>
  void interactBase(TreePiece<Data>* tp)
  {
    Visitor v;
    for (int i = 0; i < tp->interactions.size(); i++) {
      for (Node<Data>* source : tp->interactions[i]) {
        v.leaf(source, tp->leaves[i]);
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
    for (int i = 0; i < tp->leaves.size(); i++) curr_nodes[1].push_back(i);
  }
  void processLocal() {this->template processLocalBase<Visitor> (tp);}
  void interact() {this->template interactBase<Visitor> (tp);}
  virtual void traverse(Key new_key) {
    Visitor v;
    if (new_key == 1) tp->root = tp->cache_local->root;
    auto& now_ready = curr_nodes[new_key];
    std::vector<std::pair<Key, int>> curr_nodes_insertions;
    Node<Data>* start_node = tp->root;
    if (new_key > 1) {
      Node<Data>* result = tp->resumer.ckLocalBranch()->fastNodeFind(new_key);
      if (!result) CkPrintf("not good!\n");
      start_node = result;
    }
#if DEBUG
    CkPrintf("going down on key %d while its type is %d\n", new_key, start_node->type);
#endif
    for (auto bucket : now_ready) {
      std::stack<Node<Data>*> nodes;
      nodes.push(start_node);
      while (nodes.size()) {
        Node<Data>* node = nodes.top();
        nodes.pop();
#if DEBUG
        CkPrintf("tp %d, key = %d, type = %d, pe %d\n", tp->thisIndex, node->key, node->type, CkMyPe());
#endif
        switch (node->type) {
          case Node<Data>::Leaf: case Node<Data>::CachedRemoteLeaf:
            tp->interactions[bucket].push_back(node);
            break;
          case Node<Data>::Internal:
#if DELAYLOCAL
            tp->local_travs.push_back(std::make_pair(node, bucket));
            break;
#endif
          case Node<Data>::CachedBoundary: case Node<Data>::CachedRemote:
            if (v.node(node, tp->leaves[bucket])) {
              for (int i = 0; i < node->children.size(); i++) {
                nodes.push(node->children[i].load());
              }
            }
            break;
          case Node<Data>::Boundary: case Node<Data>::RemoteAboveTPKey: case Node<Data>::Remote: case Node<Data>::RemoteLeaf:
            curr_nodes_insertions.push_back(std::make_pair(node->key, bucket));
            bool prev = node->requested.exchange(true);
            if (!prev) {
              if (node->type == Node<Data>::Boundary || node->type == Node<Data>::RemoteAboveTPKey) {
                tp->global_data[node->key].requestData(tp->cache_local->thisIndex);
	      }
              else {
		tp->cache_manager[node->cm_index].requestNodes(std::make_pair(node->key, tp->cache_local->thisIndex));
	      }
            }
            std::vector<int>& list = tp->resumer.ckLocalBranch()->waiting[node->key];
            if (!list.size() || list.back() != tp->thisIndex) list.push_back(tp->thisIndex);
        }
      }
    }
    curr_nodes.erase(new_key);
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
    Node<Data>* start_node;
    if (new_key == 1) start_node = tp->cache_local->root;
    else start_node = tp->resumer.ckLocalBranch()->fastNodeFind(new_key);
    if (!start_node) CkPrintf("not good!\n");
#if DEBUG
    CkPrintf("going down on key %d while its type is %d, pe is %d\n", new_key, start_node->type, CkMyPe());
#endif
    for (auto bucket : now_ready) {
      num_waiting[bucket]--;
      std::stack<Node<Data>*> nodes;
      nodes.push(start_node);
      while (nodes.size()) {
        Node<Data>* node = nodes.top();
        nodes.pop();
#if DEBUG
        CkPrintf("tp %d, key = %d, type = %d, pe %d\n", tp->thisIndex, node->key, node->type, CkMyPe());
#endif
        switch (node->type) {
          case Node<Data>::Leaf: case Node<Data>::CachedRemoteLeaf:
            v.leaf(node, tp->leaves[bucket]);
            break;
          case Node<Data>::Internal:
          case Node<Data>::CachedBoundary: case Node<Data>::CachedRemote: {
            if (v.node(node, tp->leaves[bucket])) {
              for (int i = 0; i < node->children.size(); i++) {
                nodes.push(node->children[i].load());
              }
            }
            break;
          }
          case Node<Data>::Boundary: case Node<Data>::RemoteAboveTPKey: case Node<Data>::Remote: case Node<Data>::RemoteLeaf:
            curr_nodes_insertions.push_back(std::make_pair(node->key, bucket));
            num_waiting[bucket]++;
            bool prev = node->requested.exchange(true);
            if (!prev) {
              if (node->type == Node<Data>::Boundary || node->type == Node<Data>::RemoteAboveTPKey)
                tp->global_data[node->key].requestData(tp->cache_local->thisIndex);
              else tp->cache_manager[node->cm_index].requestNodes(std::make_pair(node->key, tp->cache_local->thisIndex));
            }
            std::vector<int>& list = tp->resumer.ckLocalBranch()->waiting[node->key];
            if (!list.size() || list.back() != tp->thisIndex) list.push_back(tp->thisIndex);
        }
      }
      if (num_waiting[bucket] == 0) {
        if (trav_tops[bucket]->parent) {
          for (int j = 0; j < BRANCH_FACTOR; j++) {
            Node<Data>* child = trav_tops[bucket]->parent->children[j].load();
            if (child == nullptr) {
              CkPrintf("child of key %d and parent type %d is nullptr\n", trav_tops[bucket]->parent->key * 8 + j, trav_tops[bucket]->parent->type);
            }
            if (child != trav_tops[bucket]) {
               if (trav_tops[bucket]->parent->type == Node<Data>::Boundary) {
                 child->type = Node<Data>::RemoteAboveTPKey;
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
private:
  TreePiece<Data>* tp;
  std::unordered_map<Key, std::vector<Node<Data>*>> curr_nodes;
public:
  DualTraverser(TreePiece<Data>* tpi, std::vector<Key> keys) : tp(tpi)
  {
    for (auto leaf : tp->leaves) curr_nodes[1].push_back(leaf);
    tp->root_from_tp_key = tp->root->findNode(tp->tp_key);
    for (auto key : keys) curr_nodes[key].push_back(tp->root_from_tp_key);
  }
  void processLocal () {}
  void interact() {this->template interactBase<Visitor>(tp);}
  virtual void traverse(Key new_key) {
    Visitor v;
    if (new_key == 1) tp->root = tp->cache_local->root;
    auto& now_ready = curr_nodes[new_key];
    std::vector<std::pair<Key, Node<Data>*>> curr_nodes_insertions;
    Node<Data>* start_node = tp->root;
    if (new_key > 1) {
      Node<Data>* result = tp->resumer.ckLocalBranch()->fastNodeFind(new_key);
      if (!result) CkPrintf("not good!\n");
      start_node = result;
    }
    Node<Data> dummy_node;
    if (!start_node) {// only possible in early dual tree
      dummy_node.key = new_key;
      dummy_node.requested.store(true);
      dummy_node.type = Node<Data>::Remote; // doesn't matter which of the 4
      start_node = &dummy_node;
    }
    for (auto payload : now_ready) {
      std::stack<std::pair<Node<Data>*, Node<Data>*>> nodes;
      nodes.push(std::make_pair(start_node, payload));
      while (nodes.size()) {
        Node<Data>* node = nodes.top().first, *currpl = nodes.top().second;
        nodes.pop();
        //CkPrintf("tp %d, key = %d, type = %d, pe %d\n", tp->thisIndex, node->key, node->type, CkMyPe());
        switch (node->type) {
          case Node<Data>::Leaf: case Node<Data>::CachedRemoteLeaf:
            for (int i = 0; i < tp->leaves.size(); i++) {
              if (tp->leaves[i] == currpl) {
                tp->interactions[i].push_back(node); // needs change
                break;
              }
            }
            break;
          case Node<Data>::Internal: case Node<Data>::CachedBoundary: case Node<Data>::CachedRemote:
            if (v.node(node, payload)) {
              for (int i = 0; i < node->children.size(); i++) {
                for (int j = 0; j < payload->children.size(); j++) {
                  nodes.push(std::make_pair(node->children[i].load(), payload->children[j].load()));
                }
              }
            }
            break;
          case Node<Data>::Boundary: case Node<Data>::RemoteAboveTPKey: case Node<Data>::Remote: case Node<Data>::RemoteLeaf:
            curr_nodes_insertions.push_back(std::make_pair(node->key, payload));
            bool prev = node->requested.exchange(true);
            if (!prev) {
              if (node->type == Node<Data>::Boundary || node->type == Node<Data>::RemoteAboveTPKey)
                tp->global_data[node->key].requestData(tp->cache_local->thisIndex);
              else tp->cache_manager[node->cm_index].requestNodes(std::make_pair(node->key, tp->cache_local->thisIndex));
            }
            std::vector<int>& list = tp->resumer.ckLocalBranch()->waiting[node->key];
            if (!list.size() || list.back() != tp->thisIndex) list.push_back(tp->thisIndex);
            break;
        }
      }
    }
    curr_nodes.erase(new_key);
    for (auto cn : curr_nodes_insertions) curr_nodes[cn.first].push_back(cn.second);
  }
};

#endif // SIMPLE_TRAVERSER_H_
