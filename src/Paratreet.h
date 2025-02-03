#ifndef PARATREET_API_H_
#define PARATREET_API_H_

#include <functional>
#include <algorithm>
#include <numeric>
#include <string>

#include "CoreFunctions.h"

#include "Reader.h"
#include "OppositeEffectsManager.h"
#include "Subtree.h"
#include "Partition.h"
#include "Configuration.h"

#include "paratreet.decl.h"
/* readonly */ extern CProxy_TreeSpec treespec;
/* readonly */ extern int peanoKey;

#define PARATREET_MAIN_VAR(m)   m##_impl_

#define PARATREET_REGISTER_MAIN(m) \
  namespace paratreet { \
  auto& PARATREET_MAIN_VAR(m) = __initMain<m>(); \
  } \
  PUPable_def(paratreet::configuration_of_t<m>);

#define PARATREET_PER_LEAF_FN_CLASS(name)   name
#define PARATREET_PER_LEAF_FN_TAG(name)   name##_tag_
#define PARATREET_PER_LEAF_FN_INST(name)  name##_inst_

// NOTE if placed in a .h file, this can lead to multiple definitions of the class
//  (perhaps we can add a PARATREET_DECLARE_PER_LEAF_FN in the future?)
#define PARATREET_REGISTER_PER_LEAF_FN(name, data, fn) \
  class PARATREET_PER_LEAF_FN_CLASS(name) : public paratreet::PerLeafAble<data> { \
  public: \
  PARATREET_PER_LEAF_FN_CLASS(name)(void) = default; \
  PARATREET_PER_LEAF_FN_CLASS(name)(CkMigrateMessage* m) : paratreet::PerLeafAble<data>(m) {} \
  PUPable_decl(PARATREET_PER_LEAF_FN_CLASS(name)); \
  virtual void operator()(SpatialNode<data>& leaf, Partition<data>* partition) override { \
  (fn)(leaf, partition); \
  } \
  }; \
  PUPable_def(PARATREET_PER_LEAF_FN_CLASS(name)); \
  PARATREET_PER_LEAF_FN_CLASS(name) PARATREET_PER_LEAF_FN_INST(name); \
  auto PARATREET_PER_LEAF_FN_TAG(name) = \
  paratreet::__addRegistrationFn(&PARATREET_PER_LEAF_FN_CLASS(name)::register_PUP_ID, #name); \


#define PARATREET_PER_LEAF_FN(name, data) CkReference<paratreet::PerLeafAble<data>>(PARATREET_PER_LEAF_FN_INST(name))

class MainChare: public CBase_MainChare {
  public:
  MainChare(CkArgMsg* m);
  void run();
};

namespace paratreet {

  template<typename Data>
  CProxy_Driver<Data> initialize(const CkCallback& cb);

  class MainBase {
    // use a shared ptr to enable custom deleters
    // and ensure the object's lifecycle is managed
    using configuration_ptr = std::shared_ptr<Configuration>;
    configuration_ptr config_;

  protected:
    MainBase(configuration_ptr&& config)
    : config_(std::forward<configuration_ptr>(config)) {}

  public:
    inline void setConfiguration(configuration_ptr&& cfg) {
      this->config_ = std::forward<configuration_ptr>(cfg);
    }

    inline paratreet::Configuration& configuration(void) {
      return *(this->config_);
    }

    virtual void __register(void) = 0;
    virtual void main(CkArgMsg*) = 0;
    virtual void run(void) = 0;

    virtual void initializeDriver(const CkCallback&) = 0;

    virtual void setDefaults(void) {}
  };

  // NOTE because this is called Main, the user's instantiation cannot be
  //    named Main for now...
  template<typename T, typename C = DefaultConfiguration>
  class Main : public MainBase {
    static const char* __makeName(const char* ty) {
      return (std::string(ty) + "<" + std::string(typeid(T).name()) + ">").c_str();
    }
  public:
    using data_type = T;
    using configuration_type = C;

    CProxy_Driver<T> driver;
    configuration_type conf;

    // pass the configuration without a deleter by default
    Main(void)
    : MainBase(std::shared_ptr<Configuration>(&conf, [](void*){})) {}

    virtual void initializeDriver(const CkCallback& cb) override {
      this->driver = initialize<T>(cb);
    }

    virtual Real getTimestep(const typename T::BoundingBox&, Real) = 0;
    virtual void preTraversalFn(ProxyPack<T>&) = 0;
    virtual void traversalFn(const typename T::BoundingBox&, ProxyPack<T>&, int) = 0;
    virtual void postIterationFn(const typename T::BoundingBox&, ProxyPack<T>&, int) = 0;

    virtual void __register(void) override {
      PUPable_reg(configuration_type);

      CkIndex_CacheManager<T>::__register(__makeName("CacheManager"), sizeof(CacheManager<T>));
      CkIndex_Resumer<T>::__register(__makeName("Resumer"), sizeof(Resumer<T>));
      CkIndex_Partition<T>::__register(__makeName("Partition"), sizeof(Partition<T>));
      CkIndex_Subtree<T>::__register(__makeName("Subtree"), sizeof(Subtree<T>));
      CkIndex_TreeCanopy<T>::__register(__makeName("TreeCanopy"), sizeof(TreeCanopy<T>));
      CkIndex_Driver<T>::__register(__makeName("Driver"), sizeof(Driver<T>));
      CkIndex_Reader<T>::__register(__makeName("Reader"), sizeof(Reader<T>));
      CkIndex_OppositeEffectsManager<T>::__register(__makeName("OppositeEffectsManager"), sizeof(OppositeEffectsManager<T>));
    }
  };

  template<typename T, typename C>
  typename Main<T, C>::configuration_type findConfiguration_(Main<T, C>&);

  template <typename T>
  using configuration_of_t = decltype(findConfiguration_(std::declval<T&>()));

  using main_type_ = std::unique_ptr<MainBase>;
  CsvExtern(main_type_, main_);

  using registration_fn_ = void (*)(const char*);
  class registration_node_ {
    registration_node_* next_;
    registration_fn_ fn_;
    const char* name_;

  public:
    registration_node_(registration_node_* next, const registration_fn_& fn, const char* name)
    : next_(next), fn_(fn), name_(name) {}

    inline registration_node_* next(void) {
      auto next = this->next_;
      (*this->fn_)(this->name_);
      delete this;
      return next;
    }
  };

  using registration_list_type_ = registration_node_*;
  CsvExtern(registration_list_type_, registration_list_);

  std::intptr_t __addRegistrationFn(const registration_fn_& fn, const char* name);

  template<typename T>
  inline MainBase& __initMain(void) {
    auto& main = CsvAccess(main_);
    new (&main) main_type_(new T());
    return *(main);
  }

  inline void __registerMain(void) {
    if (CkMyRank() == 0) {
      auto curr = CsvAccess(registration_list_);
      while (curr) { curr = curr->next(); }
      CsvAccess(main_)->__register();
    }
  }

  template<typename T>
  inline Real getTimestep(const typename T::BoundingBox& box, Real max_velocity) {
    return ((Main<T>&)*CsvAccess(main_)).getTimestep(box, max_velocity);
  }

  template<typename T>
  inline void preTraversalFn(ProxyPack<T>& pack) {
    ((Main<T>&)*CsvAccess(main_)).preTraversalFn(pack);
  }

  template<typename T>
  inline void traversalFn(const typename T::BoundingBox& box, ProxyPack<T>& pack, int iter) {
    ((Main<T>&)*CsvAccess(main_)).traversalFn(box, pack, iter);
  }

  template<typename T>
  inline void postIterationFn(const typename T::BoundingBox& box, ProxyPack<T>& pack, int iter) {
    ((Main<T>&)*CsvAccess(main_)).postIterationFn(box, pack, iter);
  }

  template<typename T>
  inline void perLeafFn(int indicator, SpatialNode<T>& node, Partition<T>* partition) {
    ((Main<T>&)*CsvAccess(main_)).perLeafFn(indicator, node, partition);
  }

  inline void setConfiguration(std::shared_ptr<Configuration>&& cfg) {
    CsvAccess(main_)->setConfiguration(std::move(cfg));
  }

  template <typename T>
  inline const T& getConfiguration(void) {
    return static_cast<const T&>(CsvAccess(main_)->configuration());
  }

  template<typename Data>
  CProxy_Driver<Data> initialize(const CkCallback& cb) {
    // Create readers
    CProxy_Reader<Data> readers = CProxy_Reader<Data>::ckNew();
    treespec = CProxy_TreeSpec::ckNew();
    CProxy_StatisticsTracker statistics = CProxy_StatisticsTracker::ckNew();
    CProxy_OppositeEffectsManager<Data> opposite_effects_manager = CProxy_OppositeEffectsManager<Data>::ckNew();

    // Create library chares
    CProxy_TreeCanopy<Data> canopy = CProxy_TreeCanopy<Data>::ckNew();
    canopy.doneInserting();
    CProxy_CacheManager<Data> cache = CProxy_CacheManager<Data>::ckNew();
    CProxy_Resumer<Data> resumer = CProxy_Resumer<Data>::ckNew();

    CProxy_Driver<Data> driver = CProxy_Driver<Data>::ckNew(readers, statistics, opposite_effects_manager, cache, resumer, canopy, CkMyPe());
    // Call the driver initialization routine (performs decomposition)
    auto& cfg = const_cast<paratreet::Configuration&>(paratreet::getConfiguration());
    peanoKey = cfg.peanoKey;
    driver.init(cb, CkReference<Configuration>(cfg));

    return driver;
  }

  template<typename Data>
  void outputSorted(const std::string& output_file, const typename Data::BoundingBox& universe, ProxyPack<Data>& pack, int iter, const std::vector<int>& indicators) {
    CkPrintf("Performing global particle sort for output\n");
    pack.partition.globalSortToReader(universe.numParticles());
    CkWaitQD();
    pack.reader.localSortByOrder(CkCallbackResumeThread());
    for (auto && indicator : indicators) {
	pack.reader[0].write(0, output_file, universe, iter, indicator, CkCallbackResumeThread());
    }
  }
}

#endif
