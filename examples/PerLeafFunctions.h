#ifndef EXAMPLES_PER_LEAF_FNS_H_
#define EXAMPLES_PER_LEAF_FNS_H_

#include "Paratreet.h"
#include "CentroidData.h"

using PerLeafBase = paratreet::PerLeafAble<CentroidData>;
using PerLeafRef = CkReference<PerLeafBase>;

class FirstCollisionFn : public PerLeafBase {
 public:
  FirstCollisionFn(void) = default;
  FirstCollisionFn(CkMigrateMessage* m) : PerLeafBase(m) {}

  PUPable_decl(FirstCollisionFn);

  virtual void perLeafFn(SpatialNode<CentroidData>& leaf,
                         Partition<CentroidData>* partition) override {
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      auto best_dt = leaf.data.pps.best_dt[pi].first;

      if (best_dt < 0.01570796326) {
        auto& partB = leaf.data.pps.best_dt[pi].second;
        auto& posA = part.position;
        auto& posB = partB.position;
        auto& velA = part.velocity;
        auto& velB = partB.velocity;
        CkPrintf(
            "deleting particles of order %d and %d that collide at dt %lf. "
            "First has position (%lf, %lf, %lf) velocity (%lf, %lf, %lf). "
            "Second has position (%lf, %lf, %lf) velocity (%lf, %lf, %lf)\n",
            part.order, partB.order, partition->time_advanced + best_dt, posA.x,
            posA.y, posA.z, velA.x, velA.y, velA.z, posB.x, posB.y, posB.z,
            velB.x, velB.y, velB.z);
        partition->deleteParticleOfOrder(part.order);
        partition->thisProxy[partB.partition_idx].deleteParticleOfOrder(
            partB.order);
      }
    }
  }
};

class SecondCollisionFn : public PerLeafBase {
 public:
  SecondCollisionFn(void) = default;
  SecondCollisionFn(CkMigrateMessage* m) : PerLeafBase(m) {}

  PUPable_decl(SecondCollisionFn);

  virtual void perLeafFn(SpatialNode<CentroidData>& leaf,
                         Partition<CentroidData>* partition) override {
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      if (part.position.lengthSquared() > 45 || part.position.z < -0.2 ||
          part.position.z > 0.2) {
        partition->deleteParticleOfOrder(part.order);
      }
    }
  }
};

class FirstSphFn : public PerLeafBase {
 public:
  FirstSphFn(void) = default;
  FirstSphFn(CkMigrateMessage* m) : PerLeafBase(m) {}

  PUPable_decl(FirstSphFn);

  virtual void perLeafFn(SpatialNode<CentroidData>& leaf,
                         Partition<CentroidData>* partition) override;
};

class SecondSphFn : public PerLeafBase {
 public:
  SecondSphFn(void) = default;
  SecondSphFn(CkMigrateMessage* m) : PerLeafBase(m) {}

  PUPable_decl(SecondSphFn);

  virtual void perLeafFn(SpatialNode<CentroidData>& leaf,
                         Partition<CentroidData>* partition) override;
};

class ThirdSphFn : public PerLeafBase {
 public:
  ThirdSphFn(void) = default;
  ThirdSphFn(CkMigrateMessage* m) : PerLeafBase(m) {}

  PUPable_decl(ThirdSphFn);

  virtual void perLeafFn(SpatialNode<CentroidData>& leaf,
                         Partition<CentroidData>* partition) override;
};

#endif
