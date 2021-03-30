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

#include "Paratreet.h"

/* readonly */ CProxy_Reader readers;
/* readonly */ CProxy_TreeSpec treespec;
/* readonly */ int n_readers;
/* readonly */ CProxy_TreeCanopy<CentroidData> centroid_calculator;
/* readonly */ CProxy_CacheManager<CentroidData> centroid_cache;
/* readonly */ CProxy_Resumer<CentroidData> centroid_resumer;
/* readonly */ CProxy_Driver<CentroidData> centroid_driver;

namespace paratreet {
    void initialize(const Configuration& conf, CkCallback cb) {
        // Create readers
        n_readers = CkNumPes();
        readers = CProxy_Reader::ckNew();
        treespec = CProxy_TreeSpec::ckNew(conf);

        // Create centroid data related chares
        centroid_calculator = CProxy_TreeCanopy<CentroidData>::ckNew();
        centroid_calculator.doneInserting();
        centroid_cache = CProxy_CacheManager<CentroidData>::ckNew();
        centroid_resumer = CProxy_Resumer<CentroidData>::ckNew();
        centroid_driver = CProxy_Driver<CentroidData>::ckNew(centroid_cache, CkMyPe());
        // Call the driver initialization routine (performs decomposition)
        centroid_driver.init(cb);
    }

    void run(CkCallback cb) {
        centroid_driver.run(cb);
    }

    void outputParticleAccelerations(BoundingBox& universe, CProxy_Partition<CentroidData>& partitions) {
        auto& output_file = treespec.ckLocalBranch()->getConfiguration().output_file;
        CProxy_Writer w = CProxy_Writer::ckNew(output_file, universe.n_particles);
        CkPrintf("Outputting particle accelerations for verification...\n");
        partitions[0].output(w, universe.n_particles, CkCallbackResumeThread());
    }
    void outputTipsy(BoundingBox& universe, CProxy_Partition<CentroidData>& partitions) {
        auto& output_file = treespec.ckLocalBranch()->getConfiguration().output_file;
        CProxy_TipsyWriter tw = CProxy_TipsyWriter::ckNew(output_file, universe.n_particles);
        CkPrintf("Outputting to Tipsy file...\n");
        partitions[0].output(tw, universe.n_particles, CkCallback::ignore);
        CkWaitQD();
        tw[0].write(0, CkCallbackResumeThread());
    }

    void updateConfiguration(const Configuration& cfg, CkCallback cb) {
        treespec.receiveConfiguration(cfg, cb);
    }
}

#include "paratreet.def.h"
