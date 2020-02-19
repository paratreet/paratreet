#ifndef PARATREET_PROXYHOLDERS_H_
#define PARATREET_PROXYHOLDERS_H_

#include "paratreet.decl.h"

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

template <typename Data>
class CProxy_Driver;

template <typename Data>
struct DPHolder {
  CProxy_Driver<Data> d_proxy;
  DPHolder (CProxy_Driver<Data> d_proxyi) :
  d_proxy(d_proxyi) {}
  DPHolder() {}
  void pup (PUP::er& p) {
    p | d_proxy;
  }
};

#endif // PARATREET_PROXYHOLDERS_H_
