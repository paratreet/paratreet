#ifndef PARATREET_API_H_
#define PARATREET_API_H_

#include <functional>
#include <algorithm>
#include <numeric>
#include <string>

#include "paratreet.decl.h"
#include "common.h"

// /* readonly */ extern CProxy_Main mainProxy;
/* readonly */ extern CProxy_Reader readers;
/* readonly */ extern CProxy_TreeSpec treespec;
/* readonly */ extern std::string input_file;
/* readonly */ extern int n_readers;
/* readonly */ extern double decomp_tolerance;
/* readonly */ extern int max_particles_per_tp; // For OCT decomposition
/* readonly */ extern int max_particles_per_leaf; // For local tree build
/* readonly */ extern int decomp_type;
/* readonly */ extern int tree_type;
/* readonly */ extern int num_iterations;
/* readonly */ extern int num_share_levels;
/* readonly */ extern int cache_share_depth;
/* readonly */ extern int flush_period;
/* readonly */ extern bool verify;
/* readonly */ extern CProxy_TreeCanopy<CentroidData> centroid_calculator;
/* readonly */ extern CProxy_CacheManager<CentroidData> centroid_cache;
/* readonly */ extern CProxy_Resumer<CentroidData> centroid_resumer;
/* readonly */ extern CProxy_CountManager count_manager;
/* readonly */ extern CProxy_Driver<CentroidData> centroid_driver;

namespace paratreet {
    using TraversalFn = std::function<void(CProxy_TreePiece<CentroidData>&,int)>;
    using PostInteractionsFn = std::function<void(CProxy_TreePiece<CentroidData>&,int)>;

    struct Configuration {
        double decomp_tolerance;
        int max_particles_per_tp; // For OCT decomposition
        int max_particles_per_leaf; // For local tree build
        int decomp_type;
        int tree_type;
        int num_iterations;
        int num_share_levels;
        int cache_share_depth;
        int flush_period;
        std::string input_file;
        TraversalFn traversalFn;
        PostInteractionsFn postInteractionsFn;
    };

    void initialize(const Configuration&, CkCallback);
    void run(CkCallback);
    void outputParticles(CProxy_TreePiece<CentroidData>&);
}

#endif
