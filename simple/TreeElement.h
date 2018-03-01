#ifndef SIMPLE_TREEELEMENT_H_
#define SIMPLE_TREEELEMENT_H_

#include "simple.decl.h"

template<typename Visitor, typename Data>
class TreeElement : public CBase_TreeElement<Visitor, Data> {
private:
  Visitor v;
  Data d;
  int wait_count;
public:
  TreeElement();
  void receiveData (Data, bool);
};

#define CK_TEMPLATES_ONLY
#include "simple.def.h"
#undef CK_TEMPLATES_ONLY

#endif // SIMPLE_TREEELEMENT_H_
