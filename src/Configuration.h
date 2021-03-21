#ifndef PARATREET_CONFIGURATION_H_
#define PARATREET_CONFIGURATION_H_

#include <functional>
#include <string>

#include "BoundingBox.h"

template<typename T>
class CProxy_Subtree;

namespace paratreet {
    enum class DecompType {
      eOct = 0,
      eSfc,
      eKd,
      eLongest,
      eInvalid = 100
    };

    enum class TreeType {
      eOct = 0,
      eOctBinary,
      eKd,
      eLongest,
      eInvalid = 100
    };

    struct Configuration {
        int min_n_subtrees;
        int min_n_partitions;
        int max_particles_per_leaf; // For local tree build
        DecompType decomp_type;
        TreeType tree_type;
        int num_iterations;
        int num_share_nodes;
        int cache_share_depth;
        int flush_period;
        int flush_max_avg_ratio;
        int lb_period;
        std::string input_file;
        std::string output_file;
#ifdef __CHARMC__
#include "pup.h"
        void pup(PUP::er &p) {
            p | min_n_subtrees;
            p | min_n_partitions;
            p | max_particles_per_leaf;
            p | decomp_type;
            p | tree_type;
            p | num_iterations;
            p | num_share_nodes;
            p | cache_share_depth;
            p | flush_period;
            p | flush_max_avg_ratio;
            p | lb_period;
            p | input_file;
            p | output_file;
        }
#endif //__CHARMC__
    };

    static std::string asString(TreeType t) {
      switch (t) {
        case TreeType::eOct:
          return "Octree";
        case TreeType::eOctBinary:
          return "OctBinaryTree";
        case TreeType::eKd:
          return "KdTree";
        case TreeType::eLongest:
          return "LongestDimTree";
        default:
          return "InvalidTreeType";
      }
    }

    static std::string asString(DecompType t) {
      switch (t) {
        case DecompType::eOct:
          return "OctDecomp";
        case DecompType::eSfc:
          return "SfcDecomp";
        case DecompType::eKd:
          return "KdDecomp";
	case DecompType::eLongest:
          return "LongestDimDecomp";
        default:
         return "InvalidDecompType";
      }
    }

    static DecompType subtreeDecompForTree(TreeType t) {
      switch (t) {
        case TreeType::eOct:
        case TreeType::eOctBinary:
          return DecompType::eOct;
        case TreeType::eKd:
          return DecompType::eKd;
        case TreeType::eLongest:
          return DecompType::eLongest;
        default:
          return DecompType::eInvalid;
      }
    }

}

#include "paratreet.decl.h"

#endif //PARATREET_CONFIGURATION_H_
