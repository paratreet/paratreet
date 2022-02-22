#ifndef PARATREET_CONFIGURATION_H_
#define PARATREET_CONFIGURATION_H_

#include <charm++.h>
#include <functional>

#include "Loadable.h"
#include "BoundingBox.h"

template<typename T>
class CProxy_Subtree;

namespace paratreet {
    enum class DecompType {
      eOct = 0, // Partitions are leaves in an octree
      eBinaryOct, // Partitions are leaves in a binary octree
      eSfc, // Partitions are chunks of a space-filling curve
      eKd, // Partitions are leaves in a k-d tree
      eLongest, // Partitions are leaves in a LongestDimension kd-tree
      eInvalid = 100
    };

    enum class TreeType {
      eOct = 0, // 8 children for every node (in 3d)
      eBinaryOct, // 2 children for every node, alternates dimension
      eKd, // k-d tree, alternates dimension
      eLongest, // k-d tree, dimension is always the longest dimension of the box
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
    struct FieldConverter<DecompType> {
      DecompType operator()(const char* val) {
        std::string s(val);
        using T = DecompType;
        if (s == "oct") {
          return T::eOct;
        } else if (s == "binoct") {
          return T::eBinaryOct;
        } else if (s == "sfc") {
          return T::eSfc;
        } else if (s == "kd") {
          return T::eKd;
        } else if (s == "longest") {
          return T::eLongest;
        } else {
          CmiAbort("-- invalid decomp type value, %s --", val);
        }
      }
    };

    template <>
    struct FieldConverter<TreeType> {
      TreeType operator()(const char* val) {
        std::string s(val);
        using T = TreeType;
        if (s == "oct") {
          return T::eOct;
        } else if (s == "binoct") {
          return T::eBinaryOct;
        } else if (s == "kd") {
          return T::eKd;
        } else if (s == "longest") {
          return T::eLongest;
        } else {
          CmiAbort("-- invalid tree type value, %s --", val);
        }
      }
    };

    struct Configuration : public PUP::able, public Loadable {
        // how many subtrees do you want, at least
        int min_n_subtrees;
        // how many partitions do you want, at least
        int min_n_partitions;
        // at most how many particles can fit in one leaf
        int max_particles_per_leaf;
        // enum for how to decompose particles to Partitions
        DecompType decomp_type;
        // enum for how to build the tree
        TreeType tree_type;
        // number of iterations to run the simulation for.
        int num_iterations;
        // how many nodes to share when requesting a node
        int num_share_nodes;
        // how many nodes of the global tree should be shared. 0 means all
        int cache_share_depth;
        // how many nodes in one pool element. nodes are stored in pools
        int pool_elem_size; 
        // after how many iterations should we flush (re-do decomposition)
        int flush_period;
        // after what decomposition (max/avg) ratio should we flush
        int flush_max_avg_ratio;
        // after how many iterations should we re-balance load
        int lb_period;
        // after how many requests per Partition should we pause that traversal
        int request_pause_interval;
        // after how many iterations should we pause that traversal
        int iter_pause_interval;
        // filename representing initial conditions
        std::string input_file;
        // filename representing output conditions
        std::string output_file;
        // Periodic boundar conditions
        int periodic;
        // Period lengths
        Vector3D<double> fPeriod;
        // Number of replicas for Ewald summation
        int nReplicas;

        // we support loading config files with "-x"
        Configuration(const char* config_arg = "-x")
        : Loadable(config_arg) { register_fields(); }

        Configuration(CkMigrateMessage *m): PUP::able(m) {}

        void register_fields(void) {
          this->register_field("nSubtreesMin", "n", min_n_subtrees);
          this->register_field("nPartitionsMin", "p", min_n_partitions);
          this->register_field("nParticlesPerLeafMax", "l", max_particles_per_leaf);
          this->register_field("achDecompType", "d", decomp_type);
          this->register_field("achTreeType", "t", tree_type);
          this->register_field("nIterations", "i", num_iterations);
          this->register_field("nShareNodes", "s", num_share_nodes);
          this->register_field("iCacheShareDepth", nullptr, cache_share_depth);
          this->register_field("iFlushPeriod", "u", flush_period);
          this->register_field("iFlushPeriodMaxAvgRatio", "r", flush_max_avg_ratio);
          this->register_field("iLbPeriod", "b", lb_period);
          this->register_field("achInFile", "f", input_file);
          this->register_field("achOutName", "v", output_file);

          this->register_field("bPeriodic", nullptr, periodic);
          this->register_field("dxPeriod", nullptr, fPeriod.x);
          this->register_field("dyPeriod", nullptr, fPeriod.y);
          this->register_field("dzPeriod", nullptr, fPeriod.z);
          this->register_field("nReplicas", nullptr, nReplicas);
        }

        int branchFactor() const {return branchFactorFromTreeType(tree_type);}

        virtual void pup(PUP::er &p) override {
            PUP::able::pup(p);
            Loadable::pup(p);
#if DEBUG
            if (p.isUnpacking()) {
              for (auto& field : this->fields_) {
                CmiPrintf("%d> deserialized field %s\n", CmiMyPe(), ((std::string)*field).c_str());
              }
            }
#endif
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
            p | periodic;
            p | fPeriod;
        }
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
