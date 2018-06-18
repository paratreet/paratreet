#ifndef SIMPLE_CACHEMANAGER_H_
#define SIMPLE_CACHEMANAGER_H_

#include "simple.decl.h"
#include "common.h"
#include "MultiMsg.h"
#include "Utility.h"
#include <map>
#include <unordered_map>
#include <vector>
#include "templates.h"
#include <atomic>

template <typename Data>
class CacheManager : public CBase_CacheManager<Data> {
public:
  CProxy_TreePiece<Data> tp_proxy;
#ifdef SMPCACHE
  CmiNodeLock tlock, dlock;
#endif
  Node<Data>* root;
  std::map<Key, Node<Data>*> tps;
  std::vector<Node<Data>*> delete_at_end;

  CacheManager() { // : root(nullptr), curr_waiting (std::map<Key, std::vector<int> >()) {}
    Node<Data>* node = new Node<Data>(1, 0, 0, NULL, 0, 0, NULL);
    node->type = Node<Data>::Boundary;
    root = node;
#ifdef SMPCACHE
    tlock = CmiCreateLock();
    dlock = CmiCreateLock();
#endif
  }

  void receiveTP(TPHolder<Data> tp_holderi) {
    tp_proxy = tp_holderi.tp_proxy;
  }

  ~CacheManager() {
    for (int i = 0; i < delete_at_end.size(); i++) delete delete_at_end[i];
    Node<Data>* temp = root;
    temp->triggerFree();
    delete temp;
  }
  int connect (Node<Data>*);
  Node<Data>* findNode(Key);
  template <typename Visitor>
  void addCache(MultiMsg<Data>*);
  template <typename Visitor>
  void addCacheHelper(Particle*, int, Node<Data>*, int);
  template <typename Visitor>
  void restoreData(Key, Data);
  template <typename Visitor>
  void resumeTraversals(Key, int);
  int insertNode(Node<Data>*, bool, bool);
  int swapIn(Node<Data>*);
};

template <typename Data>
template <typename Visitor>
void CacheManager<Data>::addCache(MultiMsg<Data>* multimsg) {
  addCacheHelper<Visitor>(multimsg->particles, multimsg->n_particles, multimsg->nodes, multimsg->n_nodes);
  delete multimsg;
}

template <typename Data>
template <typename Visitor>
void CacheManager<Data>::addCacheHelper(Particle* particles, int n_particles, Node<Data>* nodes, int n_nodes) {
  Node<Data>* first_node_placeholder = findNode(nodes[0].key);
  Node<Data>* first_node;
  if (first_node_placeholder->type != Node<Data>::CachedRemote && first_node_placeholder->type != Node<Data>::CachedRemoteLeaf) {
    int p_index = 0;
    for (int j = 0; j < n_nodes; j++) {
      Node<Data>* node = new Node<Data>(nodes[j]);
      if (j == 0) {
        node->parent = findNode(nodes[0].key)->parent;
        first_node = node;
      }
      if (node->type == Node<Data>::Leaf || node->type == Node<Data>::EmptyLeaf) {
        node->type = Node<Data>::CachedRemoteLeaf;
        if (node->n_particles) node->particles = new Particle [node->n_particles];
        for (int i = 0; i < node->n_particles; i++) {
          if (p_index < n_particles) node->particles[i] = particles[p_index++];
          else CkPrintf("yikes not good\n");
        }
      }
      else if (node->type == Node<Data>::Internal) {
        node->type = Node<Data>::CachedRemote;
      }
      insertNode(node, false, j > 0);
    }
  }
  int swap_val = swapIn(first_node);
  resumeTraversals<Visitor>(first_node_placeholder->key, swap_val);
}

template <typename Data>
template <typename Visitor>
void CacheManager<Data>::resumeTraversals (Key key, int dindex) {
  //CkPrintf("we're resuming on a node that has type %d\n", findNode(key)->type);
  //CkPrintf("resuming on key %d\n", key);
  Node<Data>* placeholder = delete_at_end[dindex];
#ifdef SMPCACHE
  CmiLock(placeholder->qlock);
#endif
  std::set<int>::iterator it1 = placeholder->waiting.begin();
  for (std::set<int>::iterator it = placeholder->waiting.begin(); it != placeholder->waiting.end(); it++) {
    //CkPrintf("restoring node %d's traversals on tp %d\n", key, *it);
    tp_proxy[*it].template goDown<Visitor>(key);
  } 
  placeholder->waiting.clear();
#ifdef SMPCACHE
  CmiUnlock(placeholder->qlock);
#endif
}

template <typename Data>
template <typename Visitor>
void CacheManager<Data>::restoreData(Key key, Data di) {
  //CkPrintf("restoring %d's data on pe %d\n", key, CkMyPe());
  Node<Data>* node = new Node<Data>();
  node->data = di;
  node->key = key;
  node->type = Node<Data>::CachedBoundary;
  node->n_children = 8;
  node->parent = (key > 1) ? findNode(key / 8) : NULL;
  if (key == 15) {if (node->parent) CkPrintf("we coo\n\n"); else CkPrintf("we NOT coo\n\n");}
  resumeTraversals<Visitor>(key, insertNode(node, true, true));
}

template <typename Data>
int CacheManager<Data>::swapIn(Node<Data>* to_swap) {
  Node<Data>* copy;
  if (to_swap->key > 1) {
    if (to_swap->parent->children.size() < 8) CkPrintf("uhoh!\n");
    copy = to_swap->parent->children[to_swap->key % 8];
    if (copy == to_swap) CkPrintf("ISSUE\n");
    to_swap->parent->children[to_swap->key % 8] = to_swap;
    if (to_swap->key >= 120 && to_swap->key < 128) CkPrintf("\n\nare we good early? %d should be 15\n\n", to_swap->parent->key);
  }
  else {
    copy = root;
    root = to_swap;
  }
#ifdef SMPCACHE
  CmiLock(dlock);
#endif
  int retval = delete_at_end.size();
  delete_at_end.push_back(copy);
#ifdef SMPCACHE
  CmiUnlock(dlock);
#endif
  return retval;
}

template <typename Data>
int CacheManager<Data>::connect(Node<Data>* node) {
  Node<Data>* copy = findNode(node->key);
  if (copy == NULL) {
#ifdef SMPCACHE
    CmiLock(tlock);
#endif
    tps.insert(std::make_pair(node->key, node));
#ifdef SMPCACHE
    CmiUnlock(tlock);
#endif
    return -1;
  }
  CkPrintf("didnt add to tps\n");
  return swapIn(node);
}

template <typename Data>
int CacheManager<Data>::insertNode(Node<Data>* node, bool above_tp, bool should_swap) {
  CkPrintf("inserting node %d of type %d with %d children\n", node->key, node->type, node->n_children);
  for (int i = 0; i < node->n_children; i++) {
    Node<Data>* new_child;
#ifdef SMPCACHE
    CmiLock(tlock);
#endif
    if (tps.count(node->key * 8 + i)) {
      new_child = tps[node->key * 8 + i];
      //CkPrintf("connected tp with key %d\n", new_child->key);
      new_child->parent = node;
      tps.erase(new_child->key);
    } 
    else {
      new_child = new Node<Data> (node->key * 8 + i, node->depth+1, 0, NULL, 0, 0, node);
      new_child->type = (above_tp) ? Node<Data>::RemoteAboveTPKey : Node<Data>::Remote;
      new_child->tp_index = node->tp_index;
    }
#ifdef SMPCACHE
    CmiUnlock(tlock);
#endif
    node->children.push_back(new_child);
  } 
  return should_swap ? swapIn(node) : -1;
}

template <typename Data>
Node<Data>* CacheManager<Data>::findNode(Key key) {
  std::vector<int> remainders;
  Key temp = key;
  while (temp >= 8) {
    remainders.push_back(temp % 8);
    temp /= 8;
  }
  Node<Data>* node = root;
  for (int i = remainders.size()-1; i >= 0; i--) {
    if (remainders[i] < node->children.size()) node = node->children[remainders[i]];
    else return NULL;
  }
  return node;
}

#endif //SIMPLE_CACHEMANAGER_H_
