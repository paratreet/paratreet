#ifndef PARATREET_COUNTMANAGER_H_
#define PARATREET_COUNTMANAGER_H_

#include "common.h"
#include "Vector3D.h"
#include "paratreet.decl.h"

#include <cstring>
#include <cmath>

struct CountManager : public CBase_CountManager {
  const double logmin;
  const double logmax;
  const int nbins;

  int* bins;

  CountManager(double min, double max, int nbins) : logmin(std::log(min)), logmax(std::log(max)), nbins(nbins) {
    bins = new int[nbins];
    std::memset(bins, 0, nbins*sizeof(int));
  }

  ~CountManager() {
    delete[] bins;
  }

  void sum(const CkCallback& cb) {
    contribute(nbins*sizeof(int), bins, CkReduction::sum_int, cb);
  }

  void count(Real d) {
    double logd = std::log(d);
    if (logd >= logmin && logd < logmax) {
      bins[int((logd-logmin)/(logmax-logmin)*nbins)] += 1;
    }
  }

  int findBin(Real a, Real b) {
    double loga = std::log(a);
    double logb = std::log(b);
    if (logb < logmin || loga >= logmax) {
      return -2; // no need to split
    } else {
      int idx1 = int((loga-logmin)/(logmax-logmin)*nbins);
      int idx2 = int((logb-logmin)/(logmax-logmin)*nbins);
      if (idx1 < 0 || idx2 < 0 || idx1 != idx2) {
          return -1; // continue to split
      } else {
          return idx1;
      }
    }
  }

};

#endif
