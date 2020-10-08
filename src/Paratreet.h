#ifndef PARATREET_API_H_
#define PARATREET_API_H_

#include <functional>
#include <algorithm>
#include <numeric>
#include <string>

#include "common.h"

#include "BoundingBox.h"
#include "ParticleMsg.h"
#include "Reader.h"
#include "TreePiece.h"
#include "GravityVisitor.h"
#include "Configuration.h"

#include "paratreet.decl.h"

/* readonly */ extern CProxy_Reader readers;
/* readonly */ extern CProxy_TreeSpec treespec;
/* readonly */ extern int n_readers;
/* readonly */ extern CProxy_TreeCanopy<CentroidData> centroid_calculator;
/* readonly */ extern CProxy_CacheManager<CentroidData> centroid_cache;
/* readonly */ extern CProxy_Resumer<CentroidData> centroid_resumer;
/* readonly */ extern CProxy_CountManager count_manager;
/* readonly */ extern CProxy_Driver<CentroidData> centroid_driver;

namespace paratreet {
    void initialize(const Configuration&, CkCallback);
    void run(CkCallback);
    void updateConfiguration(const Configuration&, CkCallback);
    void doSph(CProxy_TreePiece<CentroidData>&);
    void printNeighborList(bool, CProxy_TreePiece<CentroidData>&);
    void outputParticles(BoundingBox&, CProxy_TreePiece<CentroidData>&);
}

#endif
