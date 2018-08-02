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
  Data data;
  int wait_count;
  int tp_index;
  CProxy_TreePiece<Data> tp_proxy;
  CProxy_CacheManager<Data> cache_manager;
public:
  TreeElement();
  void reset();
  void receiveProxies(TPHolder<Data>, int, CProxy_CacheManager<Data>);
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
  wait_count = 8;
}

template <typename Data>
TreeElement<Data>::TreeElement() : data(Data()) {}

template <typename Data>
void TreeElement<Data>::reset() {
  data = Data();
  wait_count = 8;
}

template <typename Data>
void TreeElement<Data>::requestData(int cm_index) {
  if (tp_index >= 0) tp_proxy[tp_index].requestNodes(this->thisIndex, cm_index);
  else cache_manager[cm_index].restoreData(std::make_pair(this->thisIndex, data));
}

template <typename Data>
void TreeElement<Data>::receiveData (Data datai) {
  data += datai;
  wait_count--;
  if (wait_count == 0) {
    if (this->thisIndex == 1) {
      CkPrintf("%f %f %f %f\n", data.sum_mass, data.moment.x, data.moment.y, data.moment.z);
      cache_manager.restoreData(std::make_pair(1, data));
    }
    else {
      this->thisProxy[this->thisIndex >> 3].receiveData(data);
    }
  }
}

#endif // SIMPLE_TREEELEMENT_H_
