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
#include "MultiData.h"
#include <mutex>

template <typename Data>
class CacheManager : public CBase_CacheManager<Data> {
public:
  std::mutex block; //controls buffer and missing
  Node<Data>* root;
  std::unordered_map<Key, Node<Data>*> buffer, missed;
  std::vector<std::vector<Node<Data>*>> delete_at_end;
  bool isNG;
  CProxy_Resumer<Data> resumer;

  CacheManager() { // : root(nullptr), curr_waiting (std::map<Key, std::vector<int> >()) {}
    Node<Data>* node = new Node<Data>(1, 0, 0, nullptr, 0, 0, nullptr);
    node->type = Node<Data>::Boundary;
    missed.insert(std::make_pair(node->key, nullptr));
    root = node;
    isNG = this->isNodeGroup();
    delete_at_end.resize(CkNodeSize(0));
  }

  ~CacheManager() {
    for (auto& dae : delete_at_end) for (auto to_delete : dae) delete to_delete;
    Node<Data>* temp = root;
    temp->triggerFree();
    delete temp;
  }
  void connect(Node<Data>*);
  void requestNodes(std::pair<Key, int>);
  void serviceRequest(Node<Data>*, int);
  void addCache(MultiMsg<Data>*);
  void addCache(MultiData<Data>);
  Node<Data>* addCacheHelper(Particle*, int, Node<Data>*, int);
  void restoreData(std::pair<Key, Data>);
  void insertNode(Node<Data>*, bool, bool);
  void swapIn(Node<Data>*);
  void process(Key);
};

template <typename Data>
void CacheManager<Data>::addCache(MultiMsg<Data>* multimsg) {
  Node<Data>* top_node = addCacheHelper(multimsg->particles, multimsg->n_particles, multimsg->nodes, multimsg->n_nodes);
  delete multimsg;
  process(top_node->key);
}

template <typename Data>
void CacheManager<Data>::addCache(MultiData<Data> multidata) {
  //CkPrintf("adding cache for node %d\n", multidata.nodes[0].key);
  Node<Data>* top_node = addCacheHelper(multidata.particles, multidata.n_particles, multidata.nodes, multidata.n_nodes);
  process(top_node->key);
}

template <typename Data>
Node<Data>* CacheManager<Data>::addCacheHelper(Particle* particles, int n_particles, Node<Data>* nodes, int n_nodes) {
  Node<Data>* first_node_placeholder = root->findNode(nodes[0].key);
  //CkPrintf("adding cache for node %d on cm %d\n", nodes[0].key, this->thisIndex);
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
        if (first_node->key == (node->key >> 3)) node->parent = first_node;
        else node->parent = first_node->children[(node->key >> 3) % 8].load();
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
  swapIn(first_node);
  return first_node;
}

template <typename Data>
void CacheManager<Data>::requestNodes(std::pair<Key, int> param) {
  Key key = param.first;
  Node<Data>* node = root->findNode(key);
  if (!node) {
    Key temp = key;
    if (isNG) block.lock();
    while (!buffer.count(temp)) temp /= 8;
    node = buffer[temp]->findNode(key);
    if (isNG) block.unlock();
  }
  if (!node) CkPrintf("node found for key %d on cm %d\n", param.first, this->thisIndex);
  serviceRequest(node, param.second);
}

template <typename Data>
void CacheManager<Data>::serviceRequest(Node<Data>* node, int cm_index) {
  if (cm_index == this->thisIndex) return; // you'll get it later!
  if (!node) CkPrintf("UH OH\n");
  std::vector<Node<Data>> nodes;
  std::vector<Particle> sending_particles;
  nodes.push_back(*node);
  for (int i = 0; i < node->n_children; i++) {
    Node<Data>* child = node->children[i].load();
    nodes.push_back(*child);
    for (int j = 0; j < child->n_children; j++) 
      nodes.push_back(*(child->children[j].load()));
  }
  for (auto& to_send : nodes) {
    to_send.cm_index = this->thisIndex;
    if (to_send.type == Node<Data>::Leaf) {
      sending_particles.insert(sending_particles.end(), to_send.particles, to_send.particles + to_send.n_particles);
    }
  }
  MultiData<Data> multidata (sending_particles.data(), sending_particles.size(), nodes.data(), nodes.size());
  this->thisProxy[cm_index].addCache(multidata);
}

template <typename Data>
void CacheManager<Data>::restoreData(std::pair<Key, Data> param) {
  //CkPrintf("restoring data for node %d\n", param.first);
  Key key = param.first;
  Node<Data>* node = new Node<Data>(key, Node<Data>::CachedBoundary, param.second, 8, (key > 1) ? root->findNode(key / 8) : nullptr);
  insertNode(node, true, false);
  connect(node);
}

template <typename Data>
void CacheManager<Data>::swapIn(Node<Data>* to_swap) {
  //CkPrintf("swapping in node %d\n", to_swap->key);
  if (to_swap->key > 1) {
    to_swap->parent->children[to_swap->key % 8].exchange(to_swap);
  }
  else {
    std::swap(root, to_swap);
  }
  delete_at_end[CkMyRank()].push_back(to_swap);
}

template <typename Data>
void CacheManager<Data>::connect(Node<Data>* node) {
  //CkPrintf("connecting node %d\n", node->key);
  if (this->isNG) block.lock();
  auto it = missed.find(node->key);
  if (it != missed.end()) {
    node->parent = it->second;
    if (this->isNG) block.unlock();
    swapIn(node);
    process(node->key);
  }
  else buffer.insert(std::make_pair(node->key, node));
  // perhaps call process?
  // yes -- if were doing a dual tree walk
  if (this->isNG) block.unlock();
}

template <typename Data>
void CacheManager<Data>::insertNode(Node<Data>* node, bool above_tp, bool should_swap) {
  //CkPrintf("inserting node %d of type %d with %d children\n", node->key, node->type, node->n_children);
  for (int i = 0; i < node->n_children; i++) {
    Node<Data>* new_child;
    Key child_key = (node->key << 3) + i;
    bool add_placeholder = false;
    if (above_tp) {
      if (this->isNG) block.lock();
      auto it = buffer.find(child_key);
      if (it != buffer.end()) {
        new_child = it->second;
        new_child->parent = node;
      } 
      else {
        missed.insert(std::make_pair(child_key, node));
        add_placeholder = true;
      }
      if (this->isNG) block.unlock();
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
  if (!isNG) resumer[this->thisIndex].process (key);
  else for (int i = 0; i < CkNodeSize(0); i++) {
    resumer[this->thisIndex * CkNodeSize(0) + i].process(key);
  }
}

#endif //SIMPLE_CACHEMANAGER_H_
