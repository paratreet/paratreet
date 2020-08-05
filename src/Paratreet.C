#include "Paratreet.h"

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

// /* readonly */ CProxy_Main mainProxy;
/* readonly */ CProxy_Reader readers;
/* readonly */ CProxy_TreeSpec treespec;
/* readonly */ std::string input_file;
/* readonly */ int n_readers;
/* readonly */ double decomp_tolerance;
/* readonly */ int max_particles_per_tp; // For OCT decomposition
/* readonly */ int max_particles_per_leaf; // For local tree build
/* readonly */ int decomp_type;
/* readonly */ int tree_type;
/* readonly */ int num_iterations;
/* readonly */ int num_share_levels;
/* readonly */ int cache_share_depth;
/* readonly */ int flush_period;
/* readonly */ bool verify;
/* readonly */ CProxy_TreeCanopy<CentroidData> centroid_calculator;
/* readonly */ CProxy_CacheManager<CentroidData> centroid_cache;
/* readonly */ CProxy_Resumer<CentroidData> centroid_resumer;
/* readonly */ CProxy_CountManager count_manager;
/* readonly */ CProxy_Driver<CentroidData> centroid_driver;

namespace paratreet {
    void initialize(const Configuration& conf, CkCallback cb) {
        // Assign read only variables
        decomp_tolerance = conf.decomp_tolerance;
        max_particles_per_tp = conf.max_particles_per_tp;
        max_particles_per_leaf = conf.max_particles_per_leaf;
        decomp_type = conf.decomp_type;
        tree_type = conf.tree_type;
        num_iterations = conf.num_iterations;
        num_share_levels = conf.num_share_levels;
        cache_share_depth = conf.cache_share_depth;
        flush_period = conf.flush_period;
        input_file = conf.input_file;

        // Create readers
        n_readers = CkNumPes();
        readers = CProxy_Reader::ckNew();
        treespec = CProxy_TreeSpec::ckNew(tree_type, decomp_type);

        // Create centroid data related chares
        centroid_calculator = CProxy_TreeCanopy<CentroidData>::ckNew();
        centroid_cache = CProxy_CacheManager<CentroidData>::ckNew();
        centroid_resumer = CProxy_Resumer<CentroidData>::ckNew();
        centroid_driver = CProxy_Driver<CentroidData>::ckNew(centroid_cache, 0);
        count_manager = CProxy_CountManager::ckNew(0.00001, 10000, 5);

        centroid_driver.init(cb);
    }

    void run(CkCallback cb) {
        centroid_driver.run(cb);
    }

    void outputParticles(CProxy_TreePiece<CentroidData>& treepieces) {
        // std::string output_file = input_file + ".acc";
        // CProxy_Writer w = CProxy_Writer::ckNew(output_file, universe.n_particles);
        // treepieces[0].output(w, CkCallbackResumeThread());
        // CkPrintf("Outputting particle accelerations for verification...\n");
    }
}

#include "paratreet.def.h"
