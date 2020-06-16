#ifndef PARATREET_UTILITY_H_
#define PARATREET_UTILITY_H_

#include "common.h"

class Utility {

  public:
  static int mssb64_pos(Key x) {
    int n;

    if (x == Key(0)) return -1;
    n = 0;
    if (x > Key(0x00000000FFFFFFFF)) {n += 32; x >>= 32;}
    if (x > Key(0x000000000000FFFF)) {n += 16; x >>= 16;}
    if (x > Key(0x00000000000000FF)) {n += 8; x >>= 8;}
    if (x > Key(0x000000000000000F)) {n += 4; x >>= 4;}
    if (x > Key(0x0000000000000003)) {n += 2; x >>= 2;}
    if (x > Key(0x0000000000000001)) {n += 1;}
    return n;
  }

  static Key mssb64(Key x){
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    return(x & ~(x >> 1));
  }

  template<typename KEY_TYPE, typename OBJ_TYPE>
  static int binarySearchG(const KEY_TYPE &check, const OBJ_TYPE* numbers, int start, int end) {
    int lo = start;
    int hi = end;
    int mid;

    while (lo < hi) {
      mid = lo + ((hi - lo) >> 1);
      if (numbers[mid] > check) {
        hi = mid;
      }
      else {
        lo = mid + 1;
      }
    }

    return lo;
  }

  template<typename KEY_TYPE, typename OBJ_TYPE>
  static int binarySearchGE(const KEY_TYPE &check, const OBJ_TYPE* numbers, int start, int end) {
    int lo = start;
    int hi = end;
    int mid;

    while (lo < hi) {
      mid = lo + ((hi - lo) >> 1);
      if (numbers[mid] >= check) {
        hi = mid;
      }
      else {
        lo = mid + 1;
      }
    }

    return lo;
  }

  static Key getParticleLevelKey(Key k, int depth, size_t log_branch_factor) {
    return (k<<(KEY_BITS-(log_branch_factor*depth+1)));
  }

  static Key getLastParticleLevelKey(Key k, int depth, size_t log_branch_factor){
    int nshift = KEY_BITS-(log_branch_factor*depth+1);
    k <<= nshift;
    Key mask = (Key(1) << nshift);
    mask--;
    k |= mask;
    return k;
  }

  // Find depth of node from position of prepended '1' bit in node
  static int getDepthFromKey(Key k, size_t log_branch_factor){
    int nUsedBits = mssb64_pos(k);
    return (nUsedBits/log_branch_factor);
  }

  static int completeTreeSize(int levels, size_t log_branch_factor){
    return numLeaves(levels+1, log_branch_factor)-1;
  }

  static int numLeaves(int levels, size_t log_branch_factor){
    return (1<<(log_branch_factor*levels));
  }

  // is k1 a prefix of k2?
  static bool isPrefix(Key k1, Key k2, size_t log_branch_factor){
    int d1 = getDepthFromKey(k1, log_branch_factor);
    int d2 = getDepthFromKey(k2, log_branch_factor);
    if(d1 > d2) return false;
    k2 = (k2 >> ((d2 - d1) * log_branch_factor));
    return k1 == k2;
  }

  static Key removeLeadingZeros(Key k) {
    int depth = getDepthFromKey(k, 3); // doesn't matter what LBF is
    return getParticleLevelKey(k, depth, 3);
  }

};

#endif // PARATREET_UTILITY_H_
