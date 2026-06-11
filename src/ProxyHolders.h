#ifndef PARATREET_PROXYHOLDERS_H_
#define PARATREET_PROXYHOLDERS_H_

#include "paratreet.decl.h"
#include "unionFindLib.decl.h"

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
class CProxy_Partition;

template <typename Data>
struct PPHolder {
  CProxy_Partition<Data> proxy;
  PPHolder(CProxy_Partition<Data> proxy_) : proxy(proxy_) {}
  PPHolder() {}
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

template <typename Data>
class CProxy_CacheManager;

#ifdef FOF
template <typename Data>
class CProxy_LocalCalcs;
template <typename Data>
class CProxy_LocalNodeCalcs;
#endif

template <typename Data>
struct ProxyPack {
  CProxy_Driver<Data> driver;
  CProxy_Subtree<Data> subtree;
  CProxy_Partition<Data> partition;
  CProxy_CacheManager<Data> cache;
  CProxy_UnionFindLib libProxy;
#ifdef FOF
  CProxy_LocalCalcs<Data> localCalcs;
  CProxy_LocalNodeCalcs<Data> localNodeCalcs;
#endif

  ProxyPack(CProxy_Driver<Data> d, CProxy_Subtree<Data> s, CProxy_Partition<Data> p, CProxy_CacheManager<Data> c, CProxy_UnionFindLib u)
    : driver(d), subtree(s), partition(p), cache(c), libProxy(u) {}
  ProxyPack() {}
  void pup(PUP::er& p) {
    p | driver;
    p | subtree;
    p | partition;
    p | cache;
    p | libProxy;
#ifdef FOF
    p | localCalcs;
    p | localNodeCalcs;
#endif
  }
};


#endif // PARATREET_PROXYHOLDERS_H_
