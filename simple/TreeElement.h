#ifndef SIMPLE_TREEELEMENT_H_
#define SIMPLE_TREEELEMENT_H_

#include "simple.decl.h"
#include "templates.h"
#include "Node.h"

template <typename Visitor, typename Data>
class TreeElement : public CBase_TreeElement<Visitor, Data> {
private:
  Data d;
  int wait_count;
public:
  TreeElement();
  void receiveData (Data, bool);
};

extern CProxy_Main mainProxy;

template <typename Visitor, typename Data>
TreeElement<Visitor, Data>::TreeElement() {
  d = Data();
  wait_count = -1;  
}
template <typename Visitor, typename Data>
void TreeElement<Visitor, Data>::receiveData (Data di, bool if_leaf) {
  if (wait_count == -1) wait_count = (if_leaf) ? 1 : 8;
  d = d + di;
  wait_count--;
  if (wait_count == 0) {
    Visitor v (0);
    Node<Data> node;
    node.key = this->thisIndex;
    node.data = &d;
    v.node(&node);
  }
}

/*
#define CK_TEMPLATES_ONLY
#include "simple.def.h"
#undef CK_TEMPLATES_ONLY
*/

#endif // SIMPLE_TREEELEMENT_H_
