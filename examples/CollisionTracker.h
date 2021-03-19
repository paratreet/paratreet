#ifndef PARATREET_COLLISIONTRACKER_H_
#define PARATREET_COLLISIONTRACKER_H_

#include "common.h"
#include "Vector3D.h"
#include "paratreet.decl.h"

#include <cstring>
#include <cmath>

struct CollisionTracker : public CBase_CollisionTracker {
  std::set<Key> should_delete;

  void reset(const CkCallback& cb) {
    should_delete.clear();
    this->contribute(cb);
  }
  void setShouldDelete(Key key) {
    should_delete.emplace(key);
  }
};

#endif
