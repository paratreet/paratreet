#include "TreeElement.h"

extern CProxy_Main mainProxy;

template<typename Visitor, typename Data>
TreeElement<Visitor, Data>::TreeElement() :
  v (Visitor()), d (Data()), wait_count (-1) {
  //CkPrintf("%d\n", thisIndex);
}
template<typename Visitor, typename Data>
void TreeElement<Visitor, Data>::receiveData (Data di, bool if_leafi) {
  if (if_leafi) CkPrintf("woohoo\n");
  return;
  if (wait_count == -1) wait_count = (if_leafi) ? 1 : 8;
  d = d + di;
  wait_count--;
  if (wait_count == 0) {
    v.node(d, this->thisIndex);
  }
}
