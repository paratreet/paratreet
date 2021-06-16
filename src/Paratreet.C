#include "Paratreet.h"

#include "Driver.h"
#include "Reader.h"
#include "Splitter.h"
#include "Subtree.h"
#include "Partition.h"
#include "TreeCanopy.h"
#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Utility.h"
#include "CacheManager.h"
#include "Resumer.h"

/* readonly */ CProxy_Reader readers;
/* readonly */ CProxy_TreeSpec treespec;
/* readonly */ int n_readers;

namespace paratreet {
    CsvDeclare(main_type_, main_);

    void updateConfiguration(const Configuration& cfg, CkCallback cb) {
        treespec.receiveConfiguration(cfg, cb);
    }
}

#include "paratreet.def.h"
