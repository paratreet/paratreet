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
  int recv_count = 0;
  int tp_index; // If -1, sits above TreePieces
  CProxy_TreePiece<Data> tp_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CProxy_Driver<Data> d_proxy;
public:
  TreeCanopy() = default;
  void reset();
  void recvProxies(TPHolder<Data>, int, CProxy_CacheManager<Data>, DPHolder<Data>);
  void recvData(Data, int);
  void requestData(int);
};

template <typename Data>
void TreeCanopy<Data>::reset() {
  data = Data();
  recv_count = 0;
}

template <typename Data>
void TreeCanopy<Data>::recvProxies(TPHolder<Data> tp_holder, int tp_index_,
    CProxy_CacheManager<Data> cm_proxy_, DPHolder<Data> dp_holder) {
  tp_proxy = tp_holder.proxy;
  tp_index = tp_index_;
  cm_proxy = cm_proxy_;
  d_proxy = dp_holder.proxy;
}

template <typename Data>
void TreeCanopy<Data>::recvData(Data data_, int branch_factor) {
  // Accumulate data received from TreePiece or children TreeCanopies
  data += data_;

  // If data from all children has been received, send the accumulated data
  // to Driver and to the parent TreeCanopy
  if (++recv_count == branch_factor) {
    d_proxy.recvTC(std::make_pair(this->thisIndex, data));

    if (this->thisIndex == 1) {
      //CkPrintf("Total COM: %f %f %f\n", data.centroid.x, data.centroid.y, data.centroid.z);
      //cm_proxy.restoreData(std::make_pair(1, data));
    } else {
      this->thisProxy[this->thisIndex / branch_factor].recvData(data, branch_factor);
    }

    reset();
  }
}

template <typename Data>
void TreeCanopy<Data>::requestData(int cm_index) {
  if (tp_index >= 0) tp_proxy[tp_index].requestNodes(this->thisIndex, cm_index);
  else cm_proxy[cm_index].restoreData(std::make_pair(this->thisIndex, data));
}

#endif // PARATREET_TREECANOPY_H_
