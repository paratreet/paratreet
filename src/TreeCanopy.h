#ifndef PARATREET_TREECANOPY_H_
#define PARATREET_TREECANOPY_H_

#include "paratreet.decl.h"
#include "templates.h"
#include "Node.h"
#include "CacheManager.h"

template<typename Data>
class CProxy_TreePiece;

template<typename Data>
class CProxy_CacheManager;

template <typename Data>
class TreeCanopy : public CBase_TreeCanopy<Data> {
private:
  Data data;
  int wait_count;
  int tp_index;
  CProxy_TreePiece<Data> tp_proxy;
  CProxy_CacheManager<Data> cache_manager;
  CProxy_Driver<Data> driver;
public:
  TreeCanopy();
  void reset();
  void recvProxies(TPHolder<Data>, int, CProxy_CacheManager<Data>, DPHolder<Data>);
  void recvData(Data, bool);
  void requestData(int);
  void print() {
    CkPrintf("[TC %d] on PE %d from tp_index %d\n", this->thisIndex, CkMyPe(), tp_index);
  }
};

template <typename Data>
TreeCanopy<Data>::TreeCanopy() : data(Data()) {}

template <typename Data>
void TreeCanopy<Data>::reset() {
  data = Data();
  wait_count = 8;
}

template <typename Data>
void TreeCanopy<Data>::recvProxies(TPHolder<Data> tp_holder, int tp_index_,
    CProxy_CacheManager<Data> cache_manager_, DPHolder<Data> dp_holder) {
  tp_proxy = tp_holder.proxy;
  tp_index = tp_index_;
  cache_manager = cache_manager_;
  wait_count = 8;
  driver = dp_holder.proxy;
  data = Data();
}

template <typename Data>
void TreeCanopy<Data>::requestData(int cm_index) {
  if (tp_index >= 0) tp_proxy[tp_index].requestNodes(this->thisIndex, cm_index);
  else cache_manager[cm_index].restoreData(std::make_pair(this->thisIndex, data));
}

template <typename Data>
void TreeCanopy<Data>::recvData (Data datai, bool from_TP) {
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

#endif // PARATREET_TREECANOPY_H_
