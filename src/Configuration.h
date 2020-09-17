#ifndef PARATREET_CONFIGURATION_H_
#define PARATREET_CONFIGURATION_H_

#include <functional>
#include <string>

#include "BoundingBox.h"
#include "CentroidData.h"

template<typename T>
class CProxy_TreePiece;

namespace paratreet {
    using TraversalFn = std::function<void(CProxy_TreePiece<CentroidData>&,int)>;
    using PostInteractionsFn = std::function<void(BoundingBox&,CProxy_TreePiece<CentroidData>&,int)>;

    struct Configuration {
        double decomp_tolerance;
        int max_particles_per_tp; // For OCT decomposition
        int max_particles_per_leaf; // For local tree build
        int decomp_type;
        int tree_type;
        int num_iterations;
        int num_share_nodes;
        int cache_share_depth;
        int flush_period;
        Real timestep_size;
        std::string input_file;
        TraversalFn traversalFn;
        PostInteractionsFn postInteractionsFn;
#ifdef __CHARMC__
#include "pup.h"
        void pup(PUP::er &p) {
            p | decomp_tolerance;
            p | max_particles_per_tp;
            p | max_particles_per_leaf;
            p | decomp_type;
            p | tree_type;
            p | num_iterations;
            p | num_share_nodes;
            p | cache_share_depth;
            p | flush_period;
            p | input_file;
            p | timestep_size;
        }
#endif //__CHARMC__
    };
}

#include "paratreet.decl.h"

#endif //PARATREET_CONFIGURATION_H_