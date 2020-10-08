#include "Driver.h"
#include "Reader.h"
#include "Splitter.h"
#include "TreePiece.h"
#include "TreeCanopy.h"
#include "BoundingBox.h"
#include "BufferedVec.h"
#include "Utility.h"
#include "DensityVisitor.h"
#include "GravityVisitor.h"
#include "PressureVisitor.h"
#include "CountVisitor.h"
#include "CacheManager.h"
#include "CountManager.h"
#include "Resumer.h"

#include "Paratreet.h"

/* readonly */ CProxy_Reader readers;
/* readonly */ CProxy_TreeSpec treespec;
/* readonly */ std::string input_file;
/* readonly */ int n_readers;
/* readonly */ CProxy_TreeCanopy<CentroidData> centroid_calculator;
/* readonly */ CProxy_CacheManager<CentroidData> centroid_cache;
/* readonly */ CProxy_Resumer<CentroidData> centroid_resumer;
/* readonly */ CProxy_CountManager count_manager;
/* readonly */ CProxy_Driver<CentroidData> centroid_driver;

namespace paratreet {
    void initialize(const Configuration& conf, CkCallback cb) {
        // Create readers
        n_readers = CkNumPes();
        readers = CProxy_Reader::ckNew();
        treespec = CProxy_TreeSpec::ckNew(conf);

        // Set the local branch's configuration (function pointers remain intact)
        treespec.ckLocalBranch()->setConfiguration(conf);

        // Create centroid data related chares
        centroid_calculator = CProxy_TreeCanopy<CentroidData>::ckNew();
        centroid_cache = CProxy_CacheManager<CentroidData>::ckNew();
        centroid_resumer = CProxy_Resumer<CentroidData>::ckNew();
        centroid_driver = CProxy_Driver<CentroidData>::ckNew(centroid_cache, CkMyPe());
        count_manager = CProxy_CountManager::ckNew(0.00001, 10000, 5);

        // Call the driver initialization routine (performs decomposition)
        centroid_driver.init(cb);
    }

    void run(CkCallback cb) {
        centroid_driver.run(cb);
    }

    void doSph(CProxy_TreePiece<CentroidData>& treepieces) {
        treepieces[0].doSph(CkCallbackResumeThread());
    }

    void printNeighborList(bool fixed_ball, CProxy_TreePiece<CentroidData>& treepieces) {
        treepieces[0].printNeighborList(fixed_ball, CkCallbackResumeThread());
    }

    void outputParticles(BoundingBox& universe, CProxy_TreePiece<CentroidData>& treepieces) {
        std::string output_file = input_file + ".acc";
        CProxy_Writer w = CProxy_Writer::ckNew(output_file, universe.n_particles);
        treepieces[0].output(w, CkCallbackResumeThread());
        CkPrintf("Outputting particle accelerations for verification...\n");
    }

    void updateConfiguration(const Configuration& cfg, CkCallback cb) {
        treespec.receiveConfiguration(cfg, cb);
    }
}

#include "paratreet.def.h"
