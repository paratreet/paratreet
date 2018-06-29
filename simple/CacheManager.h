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
#include <mutex>

template <typename Data>
class CacheManager : public CBase_CacheManager<Data> {
public:
  CProxy_TreePiece<Data> tp_proxy;
#ifdef SMPCACHE
  std::mutex block, dlock; //block controls buffer and missing
#endif
  Node<Data>* root;
  std::map<Key, Node<Data>*> buffer;
  std::vector<Node<Data>*> delete_at_end;
  std::map<Key, Node<Data>*> missed; // not ideal but barely gets used

  CacheManager() { // : root(nullptr), curr_waiting (std::map<Key, std::vector<int> >()) {}
    Node<Data>* node = new Node<Data>(1, 0, 0, NULL, 0, 0, NULL);
    node->type = Node<Data>::Boundary;
    missed.insert(std::make_pair(node->key, (Node<Data>*) NULL));
    root = node;
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
      else {
        if (first_node->key == (node->key >> 3)) node->parent = first_node;
        else node->parent = first_node->children[(node->key >> 3) % 8];
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
  } else CkPrintf("something strange afoot\n");
  int swap_val = swapIn(first_node);
  resumeTraversals<Visitor>(first_node_placeholder->key, swap_val);
}

template <typename Data>
template <typename Visitor>
void CacheManager<Data>::resumeTraversals (Key key, int dindex) {
  Node<Data>* placeholder;
#ifdef SMPCACHE
  dlock.lock();
#endif
  placeholder = delete_at_end[dindex];
#ifdef SMPCACHE
  dlock.unlock();
  placeholder->qlock.lock();
#endif
  for (std::set<int>::iterator it = placeholder->waiting.begin(); it != placeholder->waiting.end(); it++) {
    tp_proxy[*it].template goDown<Visitor>(key);
  }
  placeholder->waiting.clear();
#ifdef SMPCACHE
  placeholder->qlock.unlock();
#endif
}

template <typename Data>
template <typename Visitor>
void CacheManager<Data>::restoreData(Key key, Data di) {
  Node<Data>* node = new Node<Data>(key, Node<Data>::CachedBoundary, di, 8, (key > 1) ? findNode(key / 8) : NULL);
  insertNode(node, true, false);
  int dindex = connect(node);
  //CkPrintf("dindex = %d\n", dindex);
  if (dindex >= 0) resumeTraversals<Visitor>(key, dindex);
}

template <typename Data>
int CacheManager<Data>::swapIn(Node<Data>* to_swap) {
  //CkPrintf("swapping in node %d\n", to_swap->key);
  Node<Data>* copy;
  if (to_swap->key > 1) {
    std::swap(to_swap, to_swap->parent->children[to_swap->key % 8]);
  }
  else {
    std::swap(root, to_swap);
  }
#ifdef SMPCACHE
  dlock.lock();
#endif
  int retval = delete_at_end.size();
  delete_at_end.push_back(to_swap); // is waiting getting copied?
#ifdef SMPCACHE
  dlock.unlock();
#endif
  return retval;
}

template <typename Data>
int CacheManager<Data>::connect(Node<Data>* node) {
  //CkPrintf("connecting on pe %d\n", CkMyPe());
#ifdef SMPCACHE
  block.lock();
#endif
  if (missed.count(node->key)) {
    node->parent = missed[node->key];
#ifdef SMPCACHE
    block.unlock();
#endif  
    return swapIn(node);
  }
  buffer.insert(std::make_pair(node->key, node));
#ifdef SMPCACHE
  block.unlock();
#endif
  return -1;
}

template <typename Data>
int CacheManager<Data>::insertNode(Node<Data>* node, bool above_tp, bool should_swap) {
  //CkPrintf("inserting node %d of type %d with %d children\n", node->key, node->type, node->n_children);
  for (int i = 0; i < node->n_children; i++) {
    Node<Data>* new_child;
    Key child_key = (node->key << 3) + i;
    bool add_placeholder = false;
    if (above_tp) {
#ifdef SMPCACHE
      block.lock();
#endif
      if (buffer.count(child_key)) {
        new_child = buffer[child_key];
        new_child->parent = node;
      } 
      else {
        missed.insert(std::make_pair(child_key, node));
        add_placeholder = true;
      }
#ifdef SMPCACHE
      block.unlock();
#endif
    }
    if (!above_tp || add_placeholder) {
      new_child = new Node<Data> (child_key, node->depth+1, 0, NULL, 0, 0, node);
      new_child->type = (above_tp) ? Node<Data>::RemoteAboveTPKey : Node<Data>::Remote;
      if (!above_tp) new_child->tp_index = node->tp_index;
    }
    node->children[i] = new_child;
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
    if (node && remainders[i] < node->children.size()) node = node->children[remainders[i]];
    else return NULL;
  }
  return node;
}

#endif //SIMPLE_CACHEMANAGER_H_
