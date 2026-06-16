#ifndef PARATREET_LOCALCALCS_H_
#define PARATREET_LOCALCALCS_H_

#include "paratreet.decl.h"
#include "Node.h"
#include "unionFindLib.h"
#include <mutex>
#include <vector>
#include <unordered_map>

template <typename Data>
struct LocalCalcs : public CBase_LocalCalcs<Data> {
  std::vector<Node<Data>*> buckets;
  // partition_idx -> {vertex array pointer, vertex count}
  std::unordered_map<int, std::pair<unionFindVertex*, int>> vertexArrays;
  CProxy_LocalNodeCalcs<Data> localNodeCalcs;

  int cross_partition_union_count = 0;
  int compress_count = 0;
  static constexpr int MAX_COMPRESSIONS = 50;

  LocalCalcs() {}
  LocalCalcs(CProxy_LocalNodeCalcs<Data> localNodeCalcs) : localNodeCalcs(localNodeCalcs) {}
  LocalCalcs(CkMigrateMessage*) {}

  void depositBucket(Node<Data>* bucket) {
    buckets.push_back(bucket);
  }

  void depositVertexArray(int partition_idx, unionFindVertex* verts, int count) {
    vertexArrays[partition_idx] = {verts, count};
  }

  // Walk the parent chain through the local vertex arrays, compressing it.
  // Returns the local root (parent==-1) or the last vertex reachable before a non-local chare.
  // is_actual_root is set true only when the returned vertex has parent==-1.
  uint64_t localFind(uint64_t vid, bool& is_actual_root) {
    auto chareOf = [](uint64_t v) -> int { return (int)(v >> 32); };
    auto idxOf   = [](uint64_t v) -> int { return (int)(v & 0xFFFFFFFF); };

    std::vector<uint64_t> path;
    uint64_t curr = vid;
    while (true) {
      auto it = vertexArrays.find(chareOf(curr));
      if (it == vertexArrays.end()) { is_actual_root = false; break; }
      int64_t par = it->second.first[idxOf(curr)].parent;
      if (par == -1) { is_actual_root = true; break; }
      path.push_back(curr);
      curr = (uint64_t)par;
    }
    for (uint64_t v : path)
      vertexArrays[chareOf(v)].first[idxOf(v)].parent = (int64_t)curr;
    return curr;
  }

  void localUnion(uint64_t vid1, uint64_t vid2) {
    auto chareOf = [](uint64_t v) -> int { return (int)(v >> 32); };
    auto idxOf   = [](uint64_t v) -> int { return (int)(v & 0xFFFFFFFF); };

    bool actual1, actual2;
    uint64_t root1 = localFind(vid1, actual1);
    uint64_t root2 = localFind(vid2, actual2);

    if (root1 == root2) return;
    if (!actual1 || !actual2) return;

    if (root1 > root2) std::swap(root1, root2);
    auto& winner = vertexArrays[chareOf(root1)].first[idxOf(root1)];
    auto& loser  = vertexArrays[chareOf(root2)].first[idxOf(root2)];
    winner.size += loser.size;
    loser.parent = (int64_t)root1;
  }

  // Compress every local vertex to point directly to either its local root
  // (parent==-1) or the remote gateway vertex at the PE boundary.
  // Resets the cross-partition union counter.
  void compressLocal() {
    auto chareOf = [](uint64_t v) -> int { return (int)(v >> 32); };
    auto idxOf   = [](uint64_t v) -> int { return (int)(v & 0xFFFFFFFF); };

    for (auto& kv : vertexArrays) {
      unionFindVertex* verts = kv.second.first;
      int count = kv.second.second;
      for (int i = 0; i < count; i++) {
        if (verts[i].parent == -1) continue;
        uint64_t curr = (uint64_t)verts[i].parent;
        while (true) {
          auto it = vertexArrays.find(chareOf(curr));
          if (it == vertexArrays.end()) break; // curr is remote gateway
          int64_t par = it->second.first[idxOf(curr)].parent;
          if (par == -1) break;               // curr is local root
          curr = (uint64_t)par;
        }
        verts[i].parent = (int64_t)curr;
      }
    }
    compress_count++;
    //CkPrintf("[LocalCalcs PE %d] compressLocal #%d\n", CkMyPe(), compress_count);
    cross_partition_union_count = 0;
  }

  void doPhase0(const CkCallback& cb) {
    this->contribute(cb);
    buckets.clear();
    vertexArrays.clear();
    cross_partition_union_count = 0;
    compress_count = 0;
  }

  void doNodeTips() {
    for( auto& kv : vertexArrays) {
      unionFindVertex* verts = kv.second.first;
      int count = kv.second.second;
      for (int i = 0; i < count; i++) {
        if (verts[i].parent == -1) continue;
       /*  uint64_t curr = (uint64_t)verts[i].parent;
        while (true) {
          auto it = vertexArrays.find((int)(curr >> 32));
          if (it == vertexArrays.end()) break; // curr is remote gateway
          int64_t par = it->second.first[(int)(curr & 0xFFFFFFFF)].parent;
          if (par == -1) break;               // curr is local root
          curr = (uint64_t)par;
        } */
        bool dummy;
        verts[i].process_tip = localNodeCalcs.ckLocalBranch()->localNodeFind((uint64_t)verts[i].parent, dummy);
      }
    }
    CmiNodeBarrier();
    // Loop: for each vertex i on my PE, verts[i].parent = verts[i].process_tip;
    for( auto& kv : vertexArrays) {
      unionFindVertex* verts = kv.second.first;
      int count = kv.second.second;
      for (int i = 0; i < count; i++) {
        if (verts[i].parent == -1) continue;
        verts[i].parent = verts[i].process_tip;
      }
    }
  }

};

template <typename Data>
struct LocalNodeCalcs : public CBase_LocalNodeCalcs<Data> {
  std::unordered_map<int, std::pair<unionFindVertex*, int>> vertexArraysNode;
  std::mutex mtx;

  LocalNodeCalcs() : mtx() {}
  LocalNodeCalcs(CkMigrateMessage*) : mtx() {}

  void depositVertexArraysNode(int partition_idx, unionFindVertex* verts, int count) {
    std::lock_guard<std::mutex> lock(mtx);
    vertexArraysNode[partition_idx] = {verts, count};
  }

  uint64_t localNodeFind(uint64_t vid, bool& is_actual_root) {
    auto chareOf = [](uint64_t v) -> int { return (int)(v >> 32); };
    auto idxOf   = [](uint64_t v) -> int { return (int)(v & 0xFFFFFFFF); };

    uint64_t curr = vid;
    while (true) {
      auto it = vertexArraysNode.find(chareOf(curr));
      if (it == vertexArraysNode.end()) { is_actual_root = false; break; }
      int64_t par = it->second.first[idxOf(curr)].parent;
      if (par == -1) { is_actual_root = true; break; }
      curr = (uint64_t)par;
    }
    return curr;
  }
};

#endif // PARATREET_LOCALCALCS_H_
