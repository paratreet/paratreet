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
#include "Subtree.h"
#include "Partition.h"
#include "Configuration.h"

#include "paratreet.decl.h"
/* readonly */ extern CProxy_Reader readers;
/* readonly */ extern CProxy_TreeSpec treespec;
/* readonly */ extern int n_readers;

namespace paratreet {
    template<typename Data>
    CProxy_Driver<Data> initialize(const Configuration& conf, CkCallback cb) {
        // Create readers
        n_readers = CkNumPes();
        readers = CProxy_Reader::ckNew();
        treespec = CProxy_TreeSpec::ckNew(conf);

        // Create centroid data related chares
        CProxy_TreeCanopy<Data> centroid_calculator = CProxy_TreeCanopy<Data>::ckNew();
        centroid_calculator.doneInserting();
        CProxy_CacheManager<Data> centroid_cache = CProxy_CacheManager<Data>::ckNew();
        CProxy_Resumer<Data> centroid_resumer = CProxy_Resumer<Data>::ckNew();

        CProxy_Driver<Data> centroid_driver = CProxy_Driver<Data>::ckNew(centroid_cache, centroid_resumer, centroid_calculator, CkMyPe());
        // Call the driver initialization routine (performs decomposition)
        centroid_driver.init(cb);

        return centroid_driver;
    }

    void updateConfiguration(const Configuration&, CkCallback);

    template<typename Data>
    void outputParticleAccelerations(BoundingBox& universe, CProxy_Partition<Data>& partitions) {
        auto& output_file = treespec.ckLocalBranch()->getConfiguration().output_file;
        CProxy_Writer w = CProxy_Writer::ckNew(output_file, universe.n_particles);
        CkPrintf("Outputting particle accelerations for verification...\n");
        partitions.output(w, universe.n_particles, CkCallback::ignore);
        CkWaitQD();
        w[0].write(CkCallbackResumeThread());
    }

    template<typename Data>
    void outputTipsy(BoundingBox& universe, CProxy_Partition<Data>& partitions) {
        auto& output_file = treespec.ckLocalBranch()->getConfiguration().output_file;
        CProxy_TipsyWriter tw = CProxy_TipsyWriter::ckNew(output_file, universe);
        CkPrintf("Outputting to Tipsy file...\n");
        partitions.output(tw, universe.n_particles, CkCallback::ignore);
        CkWaitQD();
        tw[0].write(0, CkCallbackResumeThread());
    }
}

#endif
