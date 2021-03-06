#ifndef PARATREET_API_H_
#define PARATREET_API_H_

#include <functional>
#include <algorithm>
#include <numeric>
#include <string>

#include "CoreFunctions.h"

#include "BoundingBox.h"
#include "ParticleMsg.h"
#include "Reader.h"
#include "ThreadStateHolder.h"
#include "Subtree.h"
#include "Partition.h"
#include "Configuration.h"

#include "paratreet.decl.h"
/* readonly */ extern CProxy_Reader readers;
/* readonly */ extern CProxy_TreeSpec treespec;
/* readonly */ extern CProxy_ThreadStateHolder thread_state_holder;
/* readonly */ extern int n_readers;

#define PARATREET_MAIN_VAR(m)   m##_impl

#define PARATREET_REGISTER_MAIN(m) \
    namespace paratreet { \
    auto& PARATREET_MAIN_VAR(m) = __initMain<m>(); \
    }

class MainChare: public CBase_MainChare {
  public:
    MainChare(CkArgMsg* m);
    void run();
};

namespace paratreet {

    template<typename Data>
    CProxy_Driver<Data> initialize(const Configuration& conf, CkCallback cb);

    class MainBase {
      public:
        paratreet::Configuration conf;

        virtual void __register(void) = 0;

        virtual void main(CkArgMsg*) = 0;
        virtual void run(void) = 0;

        virtual Real getTimestep(BoundingBox&, Real) = 0;

        virtual void initializeDriver(const CkCallback&) = 0;
    };

    // NOTE because this is called Main, the user's instantiation cannot be
    //      named Main for now...
    template<typename T>
    class Main : public MainBase {
        static const char* __makeName(const char* ty) {
            return (std::string(ty) + "<" + std::string(typeid(T).name()) + ">").c_str();
        }
      public:
        CProxy_Driver<T> driver;

        virtual void initializeDriver(const CkCallback& cb) override {
            this->driver = initialize<T>(this->conf, cb);
        }

        virtual void preTraversalFn(ProxyPack<T>&) = 0;
        virtual void traversalFn(BoundingBox&, ProxyPack<T>&, int) = 0;
        virtual void postIterationFn(BoundingBox&, ProxyPack<T>&, int) = 0;
        virtual void perLeafFn(int indicator, SpatialNode<T>&, Partition<T>* partition) = 0;

        virtual void __register(void) override {
            CkIndex_CacheManager<T>::__register(__makeName("CacheManager"), sizeof(CacheManager<T>));
            CkIndex_Resumer<T>::__register(__makeName("Resumer"), sizeof(Resumer<T>));
            CkIndex_Partition<T>::__register(__makeName("Partition"), sizeof(Partition<T>));
            CkIndex_Subtree<T>::__register(__makeName("Subtree"), sizeof(Subtree<T>));
            CkIndex_TreeCanopy<T>::__register(__makeName("TreeCanopy"), sizeof(TreeCanopy<T>));
            CkIndex_Driver<T>::__register(__makeName("Driver"), sizeof(Driver<T>));

            CkIndex_Reader::idx_request<T>( static_cast<void (Reader::*)(const CProxy_Subtree<T> &, int, int)>(NULL));
            CkIndex_Reader::idx_flush<T>( static_cast<void (Reader::*)(int, const CProxy_Subtree<T> &)>(NULL));
            CkIndex_Reader::idx_assignPartitions<T>( static_cast<void (Reader::*)(int, const CProxy_Partition<T> &)>(NULL));
        }
    };

    using main_type_ = std::unique_ptr<MainBase>;
    CsvExtern(main_type_, main_);

    template<typename T>
    inline MainBase& __initMain(void) {
        auto& main = CsvAccess(main_);
        new (&main) main_type_(new T());
        return *(main);
    }

    inline void __registerMain(void) {
        if (CkMyRank() == 0) {
            CsvAccess(main_)->__register();
        }
    }

    inline Real getTimestep(BoundingBox& box, Real real) {
        return CsvAccess(main_)->getTimestep(box, real);
    }

    template<typename T>
    inline void preTraversalFn(ProxyPack<T>& pack) {
        ((Main<T>&)*CsvAccess(main_)).preTraversalFn(pack);
    }

    template<typename T>
    inline void traversalFn(BoundingBox& box, ProxyPack<T>& pack, int iter) {
        ((Main<T>&)*CsvAccess(main_)).traversalFn(box, pack, iter);
    }

    template<typename T>
    inline void postIterationFn(BoundingBox& box, ProxyPack<T>& pack, int iter) {
        ((Main<T>&)*CsvAccess(main_)).postIterationFn(box, pack, iter);
    }

    template<typename T>
    inline void perLeafFn(int indicator, SpatialNode<T>& node, Partition<T>* partition) {
        ((Main<T>&)*CsvAccess(main_)).perLeafFn(indicator, node, partition);
    }

    template<typename Data>
    CProxy_Driver<Data> initialize(const Configuration& conf, CkCallback cb) {
        // Create readers
        n_readers = CkNumPes();
        readers = CProxy_Reader::ckNew();
        treespec = CProxy_TreeSpec::ckNew(conf);
        thread_state_holder = CProxy_ThreadStateHolder::ckNew();

        // Create library chares
        CProxy_TreeCanopy<Data> canopy = CProxy_TreeCanopy<Data>::ckNew();
        canopy.doneInserting();
        CProxy_CacheManager<Data> cache = CProxy_CacheManager<Data>::ckNew();
        CProxy_Resumer<Data> resumer = CProxy_Resumer<Data>::ckNew();

        CProxy_Driver<Data> driver = CProxy_Driver<Data>::ckNew(cache, resumer, canopy, CkMyPe());
        // Call the driver initialization routine (performs decomposition)
        driver.init(cb);

        return driver;
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
