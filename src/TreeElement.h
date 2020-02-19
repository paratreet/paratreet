#ifndef PARATREET_TREEELEMENT_H_
#define PARATREET_TREEELEMENT_H_

#include "paratreet.decl.h"
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
  CProxy_Driver<Data> driver;
public:
  TreeElement();
  void reset();
  void recvProxies(TPHolder<Data>, int, CProxy_CacheManager<Data>, DPHolder<Data>);
  void recvData (Data, bool);
  void requestData(int);
  void print() {
    CkPrintf("[TE %d] on PE %d from tp_index %d\n", this->thisIndex, CkMyPe(), tp_index);
  }
};

template <typename Data>
void TreeElement<Data>::recvProxies(TPHolder<Data> tp_holderi, int tp_indexi,
    CProxy_CacheManager<Data> cache_manageri, DPHolder<Data> dp_holder) {
  tp_proxy = tp_holderi.tp_proxy;
  tp_index = tp_indexi;
  cache_manager = cache_manageri;
  wait_count = 8;
  driver = dp_holder.d_proxy;
  data = Data();
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
void TreeElement<Data>::recvData (Data datai, bool from_TP) {
  if (wait_count == 0) reset();
  data += datai;
  wait_count--;
  if (wait_count == 0) {
    driver.recvTE(std::make_pair(this->thisIndex, data));
    if (this->thisIndex == 1) {
      //CkPrintf("Total COM: %f %f %f\n", data.getCentroid().x, data.getCentroid().y, data.getCentroid().z);
      //cache_manager.restoreData(std::make_pair(1, data));
    }
    else {
      this->thisProxy[this->thisIndex >> 3].recvData(data, false);
    }
  }
}

#endif // PARATREET_TREEELEMENT_H_
