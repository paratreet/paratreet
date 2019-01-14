#ifndef SIMPLE_RESUMER_H_
#define SIMPLE_RESUMER_H_

#include "simple.decl.h"
#include "common.h"
#include <unordered_map>
#include <vector>

template <typename Data>
class Resumer : public CBase_Resumer<Data> {
public:
  CProxy_TreePiece<Data> tp_proxy;
  CacheManager<Data>* cache_local;
  std::unordered_map<int, Node<Data>*> nodehash;
  std::unordered_map<Key, std::vector<int>> waiting;

  void destroy() {
    nodehash.clear();
    waiting.clear();
  }

  void process(Key key) {
    Node<Data>* node = cache_local->root->findNode(key); // try getting its ancestor from nodehash
    nodehash.insert(std::make_pair(key, node));
    auto it = waiting.find(key);
    if (it == waiting.end()) return;
    for (auto tp_index : it->second) {
      tp_proxy[tp_index].template goDown(key);
    }
    waiting.erase(it);
  }
};

#endif // SIMPLE_RESUMER_H_
