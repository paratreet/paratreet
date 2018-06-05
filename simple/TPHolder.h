#include "simple.decl.h"

template <typename Data>
class CProxy_TreePiece;

template <typename Data>
struct TPHolder {
  CProxy_TreePiece<Data> tp_proxy;
  TPHolder (CProxy_TreePiece<Data> tp_proxyi) : 
  tp_proxy(tp_proxyi) {}
  TPHolder() {}
  void pup (PUP::er& p) {
    p | tp_proxy;
  }
};


