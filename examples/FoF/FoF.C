#include <iostream>
#include "Paratreet.h"
#include "CentroidData.h"
#include "unionFindLib.h"
#include "FoFVisitor.h"
#include "FoF.decl.h"
#include "WorkMonitor.h"
#include "IdleMonitor.h"

/* readonly */ bool outputFileConfigured;
/* readonly */ CProxy_UnionFindLib libProxy;
/* readonly */ CProxy_Partition<CentroidData> partitionProxy;
/* readonly */ int peanoKey;
/* readonly */ Real linkingLength;
/* readonly */ int minVerticesPerComponent; // minimum is strictly greater than this value
int periodic;
Vector3D<Real> fPeriod;
bool verify;

// Reducer type handle populated by initIdleReducer() at node startup.
CkReduction::reducerType idleReportReducer;

// workMonitor is a Charm++ readonly so it is broadcast to ALL PEs on ALL
// processes after the main chare constructor completes.  fof_on_resume_impl
// references it from every process, so it must not be file-static.
/* readonly */ CProxy_WorkMonitor    workMonitor;
static CProxy_WorkMonitorRelay       workMonitorRelay;
static CProxy_IdleMonitorCoordinator idleMonitor;

// ------------------------------------------------------------------
// Named (non-lambda) implementations of the FoF hook function pointers.
// Defined here so they can be registered via initnode fofHooksInit()
// which runs on every OS process, not just the one that hosts the main chare.
// ------------------------------------------------------------------
static void fof_register_impl(void* trav_ptr, int part_idx, size_t trav_idx, void* proxy_hint) {
    int r = CkMyRank();
    g_trav_per_rank[r]     = static_cast<FoFTraverser*>(trav_ptr);
    g_part_idx_per_rank[r] = part_idx;
    g_trav_idx_per_rank[r] = trav_idx;
    g_work_per_rank[r].store(static_cast<FoFTraverser*>(trav_ptr)->pausedWorkSize());
    // Bug fix 2: reset per-traversal counter so K-trigger is scoped to one pass.
    g_resume_count[r] = 0;
    // Cache a valid partition proxy for this rank.  The global partitionProxy
    // readonly is only set on process 0; proxy_hint (this->thisProxy from
    // Partition::startDown) is valid on every process.
    if (proxy_hint)
        g_partition_proxy_per_rank[r] = *static_cast<CProxy_Partition<CentroidData>*>(proxy_hint);
}

static void fof_traversal_done_impl(int part_idx, size_t trav_idx) {
    int r = CkMyRank();
    // Only clear the slot if this is the traversal currently registered in it.
    // With multiple partition elements per PE the slot is overwritten by each
    // new fof_register_impl call, so a stale completion must not wipe a newer
    // partition's entry.
    if (g_part_idx_per_rank[r] == part_idx && g_trav_idx_per_rank[r] == trav_idx) {
        g_trav_per_rank[r] = nullptr;
        g_work_per_rank[r].store(0);
        g_resume_count[r]  = 0;
    }
}

static void fof_update_impl(size_t remaining) {
    g_work_per_rank[CkMyRank()].store(remaining);
}

// Per-partition resume-count histogram, indexed by partition index (0..127).
// Written only by the PE that owns the partition; no race since partitions
// are pinned to PEs for the lifetime of a traversal.
static int g_part_resume_count[256] = {};

static void fof_on_resume_impl(void* trav_ptr, int part_idx, size_t trav_idx) {
    int r = CkMyRank();
    if (g_help_armed[r]) return;
    int cnt = ++g_resume_count[r];
    // Post-help cooldown: HELP_COOLDOWN resume calls must happen (doing real
    // traversal work) before the trigger logic can fire again.
    if (cnt <= 0) return;
    if (part_idx >= 0 && part_idx < 256) ++g_part_resume_count[part_idx];
    // Print per-partition histogram at fixed count so the tail-partition
    // distribution is visible even when the K-trigger never fires.
    if (cnt == 900) {
        // Collect top-5 per-partition resume counts on this process.
        int top_idx[5] = {-1,-1,-1,-1,-1};
        int top_cnt[5] = {0,0,0,0,0};
        for (int p = 0; p < 256; p++) {
            int c = g_part_resume_count[p];
            if (c <= top_cnt[4]) continue;
            top_cnt[4] = c; top_idx[4] = p;
            for (int j = 3; j >= 0 && top_cnt[j+1] > top_cnt[j]; j--) {
                std::swap(top_cnt[j], top_cnt[j+1]);
                std::swap(top_idx[j], top_idx[j+1]);
            }
        }
        printf("[part-hist] PE %d at cnt=%d top partitions:", CkMyPe(), cnt);
        for (int j = 0; j < 5 && top_idx[j] >= 0; j++)
            printf(" p%d=%d", top_idx[j], top_cnt[j]);
        printf("\n");
    }

    // Bug fix 3: primary trigger is "last PE standing" — I'm the only PE on
    // this process that still has a registered (non-null) traverser.
    // K-trigger is kept as a fallback in case fof_traversal_done is missed.
    int n = CkNodeSize(CkMyNode());
    int active = 0;
    for (int i = 0; i < n; i++)
        if (g_trav_per_rank[i] != nullptr) active++;
    bool is_last_pe = (active <= 1);

    // Don't fire unless we're alone or the K-trigger fallback has fired.
    if (!is_last_pe && cnt < PARALLEL_HELP_K) return;

    // Diagnostic: print once at the K-trigger threshold (whether or not we're
    // the last PE) so we can see the queue depth and active count at that point.
    if (cnt == PARALLEL_HELP_K) {
        FoFTraverser* self_trav = static_cast<FoFTraverser*>(trav_ptr);
        printf("[tail-check] PE %d rank %d cnt=%d: paused=%zu active_on_process=%d\n",
               CkMyPe(), r, cnt, self_trav->pausedWorkSize(), active);
    }

    // Skip if the queue is too small to be worth the barrier overhead.
    // Require at least one entry per PE so every helper can get work.
    FoFTraverser* self_trav = static_cast<FoFTraverser*>(trav_ptr);
    if (self_trav->pausedWorkSize() < (size_t)n) return;

    bool expected = false;
    if (!g_parallel_triggered.compare_exchange_strong(expected, true)) return;
    int first = CkNodeFirst(CkMyNode());
    FoFTraverser* trav = static_cast<FoFTraverser*>(trav_ptr);
    g_trav_per_rank[r]     = trav;
    g_part_idx_per_rank[r] = part_idx;
    g_trav_idx_per_rank[r] = trav_idx;
    g_work_per_rank[r].store(trav->pausedWorkSize());
    int    source_rank = r;
    size_t max_work    = g_work_per_rank[r].load();
    for (int i = 0; i < n; i++) {
        size_t w = g_work_per_rank[i].load();
        if (w > max_work && g_trav_per_rank[i] != nullptr) {
            max_work    = w;
            source_rank = i;
        }
    }
    FoFTraverser* src_trav = g_trav_per_rank[source_rank];
    if (!src_trav || src_trav->getPausedWork().empty()) {
        printf("[tail-check] PE %d rank %d: trigger ABORTED — source rank %d paused=%zu (src_trav=%s)\n",
               CkMyPe(), r, source_rank,
               src_trav ? src_trav->getPausedWork().size() : 0,
               src_trav ? "non-null" : "null");
        g_parallel_triggered.store(false);
        return;
    }
    g_help_armed[r] = true;
    src_trav->steal_cursor          = 0;
    src_trav->parallel_phase_active = true;
    for (int i = 0; i < n; i++)
        workMonitor[first + i].helpSource(source_rank);
    printf("[K-trigger] PE %d triggered: source rank %d (PE %d), work=%zu, count=%d\n",
           CkMyPe(), source_rank, first + source_rank, max_work, cnt);
}

// Called by Charm++ on every process (initnode) before any PE threads start.
// Sets function pointers so all processes have working hooks.
void fofHooksInit() {
    paratreet::fof_register_traverser    = fof_register_impl;
    paratreet::fof_update_traversal_work = fof_update_impl;
    paratreet::fof_on_resume             = fof_on_resume_impl;
    paratreet::fof_traversal_done        = fof_traversal_done_impl;
}

// Called on every node before main() via the initnode declaration in FoF.ci.
void initIdleReducer() {
  idleReportReducer = CkReduction::addReducer(mergeIdleStats);
}

using namespace paratreet;

static void initialize() {
  BoundingBox::registerReducer();
}

class FoF : public paratreet::Main<CentroidData> {
  void main(CkArgMsg* m) override {
    // Initialize readonly variables
    if (conf.input_file.empty()) 
      CkPrintf("warning: no input file provided\n");
    CkAssert(!conf.input_file.empty());

    peanoKey = 3;
    linkingLength = conf.linking_length;
    fPeriod=conf.fPeriod;
    periodic = conf.periodic;

    minVerticesPerComponent = conf.min_vertices_per_component;

    // Process command line arguments
    int c;
    std::string input_str;

    while ((c = getopt(m->argc, m->argv, "mec:j:")) != -1) {
      switch (c) {
        case 'm':
          peanoKey = 0; // morton space filling curves
          break;
        
        default:
          CkPrintf("Usage: %s\n", m->argv[0]);
          CkPrintf("\t-n [minimum number of treepieces]\n");
          CkPrintf("\t-p [minimum number of partitions]\n");
          CkPrintf("\t-l [maximum number of particles per leaf]\n");
          CkPrintf("\t-d [decomposition type: oct, sfc, kd]\n");
          CkPrintf("\t-t [tree type: oct, bin, kd]\n");
          CkPrintf("\t-i [number of iterations]\n");
          CkPrintf("\t-s [number of shared tree levels]\n");
          CkPrintf("\t-u [flush period]\n");
          CkPrintf("\t-r [flush threshold for Subtree max_average ratio]\n");
          CkPrintf("\t-b [load balancing period]\n");

          CkPrintf("\t-e [set a gravatational softening for all particles]\n");
          CkPrintf("\t-f [input file]\n");
          CkPrintf("\t-v [output file prefix]\n");

          CkPrintf("\t-ll [linking length]\n");
          CkPrintf("\t-pbc [periodic boundary conditions]\n");
          CkPrintf("\t-px (-py, -pz) [x period (y period, z period)]\n");
          CkPrintf("\t-c [minimum vertices per component]\n");
          CkExit();
      }
    }
    delete m;

    // Print configuration
    CkPrintf("\n[PARATREET]\n");
    if (conf.input_file.empty()) CkAbort("Input file unspecified");
    CkPrintf("Input file: %s\n", conf.input_file.c_str());
    CkPrintf("Output file prefix: %s\n", conf.output_file.empty() ? "output file prefix not provided" : conf.output_file.c_str());
    verify = conf.output_file.empty() ? false : true;
    CkPrintf("Decomposition type: %s\n", paratreet::asString(conf.decomp_type).c_str());
    CkPrintf("Tree type: %s\n", paratreet::asString(conf.tree_type).c_str());
    CkPrintf("Minimum number of subtrees: %d\n", conf.min_n_subtrees);
    CkPrintf("Minimum number of partitions: %d\n", conf.min_n_partitions);
    CkPrintf("Maximum number of particles per leaf: %d\n", conf.max_particles_per_leaf);
    CkPrintf("Linking length for friends-of-friends: %f\n", conf.linking_length);
    CkPrintf("Minimum vertices per group for friends-of-friends is strictly greater than: %d\n", minVerticesPerComponent);
    
    // Create idle-monitoring infrastructure and wire it into Driver's traversal
    // loop via the FoFHooks function pointers.
    workMonitor = CProxy_WorkMonitor::ckNew();
    workMonitorRelay = CProxy_WorkMonitorRelay::ckNew();
    workMonitorRelay.init(workMonitor);
    idleMonitor = CProxy_IdleMonitorCoordinator::ckNew();
    // Reset all PE idle accumulators first (synchronous broadcast), then start
    // the monitoring cycle.  The lambda runs inside Driver::run() which is a
    // [threaded] entry method, so CkCallbackResumeThread() is legal here.
    paratreet::fof_start_idle_monitor = []() {
        workMonitor.resetIdleTime(CkCallbackResumeThread());
        idleMonitor.start(workMonitor, workMonitorRelay);
    };
    paratreet::fof_stop_idle_monitor  = []() { idleMonitor.stop(); };

    // main::initializeDriver() will be run after main exits
    // After that main::run() is ran. See Paratreet.C::MainChare class
  }


  void setDefaults(void) {
    conf.min_n_subtrees = CkNumPes() * 16; // default from ChaNGa
    conf.min_n_partitions = CkNumPes() * 16;
    conf.max_particles_per_leaf = 12; // default from ChaNGa
    conf.decomp_type = paratreet::DecompType::eKd;
    conf.tree_type = paratreet::TreeType::eKd;
    conf.num_iterations = 2; //just load balance in iteration 0
    conf.num_share_nodes = 0; // 3;
    conf.cache_share_depth = 3;
    conf.pool_elem_size = 1024;
    conf.flush_period = 0;
    conf.flush_max_avg_ratio = 10.;
    conf.lb_period = 1;
    conf.request_pause_interval = 20;
    conf.iter_pause_interval = 32;
    conf.min_vertices_per_component = 2; // default from ChaNGa
    conf.linking_length = 0.2; // default from ChaNGa
  }

  // -------------------
  // Traversal functions
  // -------------------
  void preTraversalFn(ProxyPack<CentroidData>& proxy_pack) override {
    // The size of the starter pack of data loaded by the cache manager is
    // specified in Configuration.cache_share_depth
    proxy_pack.driver.loadCache(CkCallbackResumeThread());

    // Store proxies as global variables for access
    libProxy = proxy_pack.libProxy;
    partitionProxy = proxy_pack.partition;

    //load balancing: use subtree volumes to help with load balancing
    //Calculate the subtree's volumes from CentroidData->box->volume()
    //then send these volumes to the charm runtime with setCpuTime
    //then make the subtree call atSync to migrate based on these volumes
    //partitionProxy.pauseForLB();

  }

  void traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) override {

    workMonitor.setLocalCalcsProxy(proxy_pack.localCalcs);
    workMonitor.setLocalNodeCalcsProxy(proxy_pack.localNodeCalcs);
    
    //only need to look at cubes that are almost touching (N=1)
    if(!periodic)
    {
      proxy_pack.partition.template startDown<FoFVisitor>(FoFVisitor(Vector3D<Real>(0,0,0), iter, proxy_pack.localCalcs));
    }
    else
    {
      for (int X = -1; X <= 1; ++X) {
        for (int Y = -1; Y <= 1; ++Y) {
          for (int Z = -1; Z <= 1; ++Z) {
            Vector3D<Real> offset(X * fPeriod.x, Y * fPeriod.y, Z * fPeriod.z);
            proxy_pack.partition.template startDown<FoFVisitor>(FoFVisitor(offset, iter, proxy_pack.localCalcs));
          }
        }
      }
    }
  }

  void postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) override {
    //if(iter==2)
    //{
    CkPrintf("[Main] Inverted trees constructed for unionFindLib. Performing components detection\n");
    int startTime = CkWallTimer();
    libProxy.find_components(CkCallbackResumeThread());

    CkPrintf("[Main] Components identified. Prune components with too few particles now\n");
    CkPrintf("[Main] Components detection time: %f\n", CkWallTimer() - startTime);
    startTime = CkWallTimer();
    // min vertices per component is strictly greater than minVerticesPerComponent
    libProxy.prune_components(minVerticesPerComponent, CkCallbackResumeThread());

    CkPrintf("[Main] Components pruned. Labeling particles with component numbers\n");
    CkPrintf("[Main] Component pruning time: %f\n", CkWallTimer() - startTime);
    startTime = CkWallTimer();
    partitionProxy.getConnectedComponents(CkCallbackResumeThread());
  
    CkPrintf("[Main] Components pruned and labeled. Outputting results of friends-of-friends\n");
    CkPrintf("[Main] Component labeling time: %f\n", CkWallTimer() - startTime);
    startTime = CkWallTimer();
    if(verify) paratreet::outputParticleAccelerations(universe, partitionProxy);

    CkPrintf("[Main] Output complete for friends-of-friends\n");
    CkPrintf("[Main] Writing to output time: %f\n", CkWallTimer() - startTime);
    //}
  }
  
  Real getTimestep(BoundingBox& universe, Real max_velocity) override {
    return 0;
  }

  void run() override {
    driver.run(CkCallbackResumeThread());
    CkExit();
  }
};

PARATREET_REGISTER_MAIN(FoF);
#include "templates.h"
#include "FoF.def.h"
