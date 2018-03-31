#ifndef SIMPLE_TREEELEMENT_H_
#define SIMPLE_TREEELEMENT_H_

#include "simple.decl.h"
#include "templates.h"
#include "Node.h"

template<typename Data>
class CProxy_TreePiece;

template <typename Data>
class TreeElement : public CBase_TreeElement<Data> {
private:
  Data d;
  int wait_count;
  int tp_index;
  CProxy_TreePiece<Data> tp_proxy;
public:
  TreeElement();
  template <typename Visitor>
  void receiveData (CProxy_TreePiece<Data>, Data, int);
  template <typename Visitor> 
  void requestData(int);
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
template <typename Visitor>
void TreeElement<Data>::requestData(int index) {
  if (tp_index >= 0) tp_proxy[tp_index].template requestNodes<Visitor>(this->thisIndex, index);
  else tp_proxy[index].template restoreData<Visitor>(this->thisIndex, d);
}

template <typename Data>
template <typename Visitor>
void TreeElement<Data>::receiveData (CProxy_TreePiece<Data> tp_proxyi, Data di, int tp_indexi) {
  tp_proxy = tp_proxyi;
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
