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
  CProxy_CacheManager<Data> cache_manager;
public:
  TreeElement();
  void reset();
  void receiveProxies(TPHolder<Data>, int, CProxy_CacheManager<Data>);
  template <typename Visitor>
  void receiveData (Data);
  void requestData(int);
  void print() {
    CkPrintf("[TE %d] on PE %d from tp_index %d\n", this->thisIndex, CkMyPe(), tp_index);
  }
};

extern CProxy_Main mainProxy;

template <typename Data>
void TreeElement<Data>::receiveProxies(TPHolder<Data> tp_holderi, int tp_indexi, CProxy_CacheManager<Data> cache_manageri) {
  tp_proxy = tp_holderi.tp_proxy;
  tp_index = tp_indexi;
  cache_manager = cache_manageri;
  wait_count = (tp_index >= 0) ? 1 : 8;
}

template <typename Data>
TreeElement<Data>::TreeElement() : d(Data()) {
}

template <typename Data>
void TreeElement<Data>::reset() {
  d = Data();
  wait_count = -1;
}

template <typename Data>
void TreeElement<Data>::requestData(int cm_index) {
  if (tp_index >= 0) tp_proxy[tp_index].requestNodes(this->thisIndex, cm_index);
  else cache_manager[cm_index].restoreData(std::make_pair(this->thisIndex, d));
}

template <typename Data>
template <typename Visitor>
void TreeElement<Data>::receiveData (Data di) {
  d = d + di;
  wait_count--;
  if (wait_count == 0) {
    Visitor v (tp_proxy, -1);
    Node<Data> node (this->thisIndex, Node<Data>::Boundary, d, 0, NULL);
    v.node(&node);
  }
}

#endif // SIMPLE_TREEELEMENT_H_
