#ifndef PARATREET_PROXYHOLDERS_H_
#define PARATREET_PROXYHOLDERS_H_

#include "paratreet.decl.h"

template <typename Data>
class CProxy_TreePiece;

template <typename Data>
struct TPHolder {
  CProxy_TreePiece<Data> tp_proxy;
  TPHolder(CProxy_TreePiece<Data> tp_proxy_) : tp_proxy(tp_proxy_) {}
  TPHolder() {}
  void pup(PUP::er& p) {
    p | tp_proxy;
  }
};

template <typename Data>
class CProxy_TreeCanopy;

template <typename Data>
struct TCHolder {
  CProxy_TreeCanopy<Data> tc_proxy;
  TCHolder(CProxy_TreeCanopy<Data> tc_proxy_) : tc_proxy(tc_proxy_) {}
  TCHolder() {}
  void pup(PUP::er& p) {
    p | tc_proxy;
  }
};

template <typename Data>
class CProxy_Driver;

template <typename Data>
struct DPHolder {
  CProxy_Driver<Data> d_proxy;
  DPHolder(CProxy_Driver<Data> d_proxy_) : d_proxy(d_proxy_) {}
  DPHolder() {}
  void pup(PUP::er& p) {
    p | d_proxy;
  }
};

#endif // PARATREET_PROXYHOLDERS_H_
