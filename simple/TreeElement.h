#ifndef SIMPLE_TREEELEMENT_H_
#define SIMPLE_TREEELEMENT_H_

#include "simple.decl.h"
#include "templates.h"
#include "Node.h"
#include "CacheManager.h"

template<typename Data>
class CProxy_TreePiece;

template<typename Data>
class CProxy_CacheManager;

template <typename Data>
class TreeElement : public CBase_TreeElement<Data> {
private:
  Data d;
  int wait_count;
  int tp_index;
  CProxy_TreePiece<Data> tp_proxy;
#ifdef SMPCACHE
  std::set<int> filled_requests;
#endif
public:
  TreeElement();
  void reset();
  template <typename Visitor>
  void receiveData (TPHolder<Data>, Data, int);
  template <typename Visitor> 
  void requestData(CProxy_CacheManager<Data>, int);
  void print() {
    CkPrintf("[TE %d] with tp_index %d\n", this->thisIndex, tp_index);
  }
};

extern CProxy_Main mainProxy;

template <typename Data>
TreeElement<Data>::TreeElement() {
  d = Data();
  wait_count = -1;
}

template <typename Data>
void TreeElement<Data>::reset() {
  d = Data();
  wait_count = -1;
}

template <typename Data>
template <typename Visitor>
void TreeElement<Data>::requestData(CProxy_CacheManager<Data> cache_manager, int cm_index) {
  if (tp_index >= 0) tp_proxy[tp_index].template requestNodes<Visitor>(this->thisIndex, cache_manager, cm_index);
  else {
#ifdef SMPCACHE
    if (!filled_requests.count(cm_index)) cache_manager[cm_index].template restoreData<Visitor> (this->thisIndex, d); 
    filled_requests.insert(cm_index);
#else
    cache_manager[cm_index].template restoreData<Visitor>(this->thisIndex, d);
#endif
  }
}

template <typename Data>
template <typename Visitor>
void TreeElement<Data>::receiveData (TPHolder<Data> tp_holderi, Data di, int tp_indexi) {
  tp_proxy = tp_holderi.tp_proxy;
  tp_index = tp_indexi;
  if (wait_count == -1) wait_count = (tp_index >= 0) ? 1 : 8; // tps need 1 message
  d = d + di;
  wait_count--;
  if (wait_count == 0) {
    Visitor v (tp_proxy);
    Node<Data> node;
    node.key = this->thisIndex;
    node.type = Node<Data>::Boundary;
    node.data = d;
    v.node(&node);
  }
}

#endif // SIMPLE_TREEELEMENT_H_
