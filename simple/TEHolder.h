#include "simple.decl.h"

template <typename Data>
class CProxy_TreeElement;

template <typename Data>
struct TEHolder {
  CProxy_TreeElement<Data> te_proxy;
  TEHolder (CProxy_TreeElement<Data> te_proxyi) : 
  te_proxy(te_proxyi) {}
  TEHolder() {}
  void pup (PUP::er& p) {
    p | te_proxy;
  }
};


