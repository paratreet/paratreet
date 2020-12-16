#ifndef PARATREET_PROXYHOLDERS_H_
#define PARATREET_PROXYHOLDERS_H_

#include "paratreet.decl.h"

template <typename Data>
class CProxy_Subtree;

template <typename Data>
struct TPHolder {
  CProxy_Subtree<Data> proxy;
  TPHolder(CProxy_Subtree<Data> proxy_) : proxy(proxy_) {}
  TPHolder() {}
  void pup(PUP::er& p) {
    p | proxy;
  }
};

template <typename Data>
class CProxy_TreeCanopy;

template <typename Data>
struct TCHolder {
  CProxy_TreeCanopy<Data> proxy;
  TCHolder(CProxy_TreeCanopy<Data> proxy_) : proxy(proxy_) {}
  TCHolder() {}
  void pup(PUP::er& p) {
    p | proxy;
  }
};

template <typename Data>
class CProxy_Driver;

template <typename Data>
struct DPHolder {
  CProxy_Driver<Data> proxy;
  DPHolder(CProxy_Driver<Data> proxy_) : proxy(proxy_) {}
  DPHolder() {}
  void pup(PUP::er& p) {
    p | proxy;
  }
};

#endif // PARATREET_PROXYHOLDERS_H_
