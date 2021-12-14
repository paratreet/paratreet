#ifndef PARATREET_CONFIGURATION_H_
#define PARATREET_CONFIGURATION_H_

#include <functional>
#include <string>

#include "Loadable.h"
#include "BoundingBox.h"

template<typename T>
class CProxy_Subtree;

namespace paratreet {
    enum class DecompType {
      eOct = 0,
      eBinaryOct,
      eSfc,
      eKd,
      eLongest,
      eInvalid = 100
    };

    enum class TreeType {
      eOct = 0,
      eBinaryOct,
      eKd,
      eLongest,
      eInvalid = 100
    };

    inline int branchFactorFromTreeType(TreeType t) {
      switch (t) {
        case TreeType::eOct: return 8;
        case TreeType::eBinaryOct: return 2;
        case TreeType::eKd: return 2;
        case TreeType::eLongest: return 2;
        default: {
          CkAbort("Tree type is invalid");
          return 0;
        }
      };
    };

    template <>
    inline void Field<DecompType>::accept(const Value& value) {
      auto s = (std::string)value;
      using T = DecompType;
      if (s == "oct") {
        this->storage_ = T::eOct;
      } else if (s == "binoct") {
        this->storage_ = T::eBinaryOct;
      } else if (s == "sfc") {
        this->storage_ = T::eSfc;
      } else if (s == "kd") {
        this->storage_ = T::eKd;
      } else if (s == "longest") {
        this->storage_ = T::eLongest;
      } else {
        CmiPrintf("Invalid value %s for %s!", s.c_str(), this->name().c_str());
        CmiAbort("Invalid value -- see above");
      }
    }

    template <>
    inline void Field<TreeType>::accept(const Value& value) {
      auto s = (std::string)value;
      using T = TreeType;
      if (s == "oct") {
        this->storage_ = T::eOct;
      } else if (s == "binoct") {
        this->storage_ = T::eBinaryOct;
      } else if (s == "kd") {
        this->storage_ = T::eKd;
      } else if (s == "longest") {
        this->storage_ = T::eLongest;
      } else {
        CmiPrintf("Invalid value %s for %s!", s.c_str(), this->name().c_str());
        CmiAbort("Invalid value -- see above");
      }
    }

    struct Configuration : public PUP::able, public Loadable {
        int min_n_subtrees;
        int min_n_partitions;
        int max_particles_per_leaf; // For local tree build
        DecompType decomp_type;
        TreeType tree_type;
        int branchFactor() const {return branchFactorFromTreeType(tree_type);}
        int num_iterations;
        int num_share_nodes;
        int cache_share_depth;
        int pool_elem_size;
        int flush_period;
        int flush_max_avg_ratio;
        int lb_period;
        int request_pause_interval;
        int iter_pause_interval;
        std::string input_file;
        std::string output_file;

        Configuration(void) { this->register_fields(); }
        Configuration(CkMigrateMessage *m): PUP::able(m) {}

        void register_fields(void) {
          this->register_field("nSubtreesMin", false, min_n_subtrees);
          this->register_field("nPartitionsMin", false, min_n_partitions);
          this->register_field("nParticlesPerLeafMax", false, max_particles_per_leaf);
          this->register_field("achDecompType", false, decomp_type);
          this->register_field("achTreeType", false, tree_type);
          this->register_field("nIterations", false, num_iterations);
          this->register_field("nShareNodes", false, num_share_nodes);
          this->register_field("iCacheShareDepth", false, cache_share_depth);
          this->register_field("iFlushPeriod", false, flush_period);
          this->register_field("iFlushPeriodMaxAvgRatio", false, flush_max_avg_ratio);
          this->register_field("iLbPeriod", false, lb_period);
          this->register_field("achInputFile", false, input_file);
          this->register_field("achOutputFile", false, output_file);
        }

#ifdef __CHARMC__
#include "pup.h"
        virtual void pup(PUP::er &p) override {
            PUP::able::pup(p);
            p | min_n_subtrees;
            p | min_n_partitions;
            p | max_particles_per_leaf;
            p | decomp_type;
            p | tree_type;
            p | num_iterations;
            p | num_share_nodes;
            p | cache_share_depth;
            p | pool_elem_size;
            p | flush_period;
            p | flush_max_avg_ratio;
            p | lb_period;
            p | request_pause_interval;
            p | iter_pause_interval;
            p | input_file;
            p | output_file;
        }
#endif //__CHARMC__
    };

    // workaround to enable custom configuration types
    // contains "PUPable_decl_inside" so Configuration
    // does not have to -- will be undefined if users
    // provide their own!
    struct DefaultConfiguration : Configuration {
      DefaultConfiguration(void) = default;
      DefaultConfiguration(CkMigrateMessage* m) : Configuration(m) {}
      PUPable_decl_inside(DefaultConfiguration);
    };

    static std::string asString(TreeType t) {
      switch (t) {
        case TreeType::eOct:
          return "Octree";
        case TreeType::eBinaryOct:
          return "BinaryOctTree";
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
        case DecompType::eBinaryOct:
          return "BinaryOctDecomp";
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
          return DecompType::eOct;
        case TreeType::eBinaryOct:
          return DecompType::eBinaryOct;
        case TreeType::eKd:
          return DecompType::eKd;
        case TreeType::eLongest:
          return DecompType::eLongest;
        default:
          return DecompType::eInvalid;
      }
    }

    template<typename T = Configuration>
    inline const T& getConfiguration(void);

    inline void setConfiguration(std::shared_ptr<Configuration>&&);
}

#endif //PARATREET_CONFIGURATION_H_
