#ifndef PARATREET_WORKMONITOR_H_
#define PARATREET_WORKMONITOR_H_

// WorkMonitor.h must be included AFTER Traverser.h, FoFVisitor.h, LocalCalcs.h,
// and FoF.decl.h.  All of those are pulled in transitively by Paratreet.h before
// this header is reached in FoF.C.

#include <algorithm>
#include <atomic>
#include <functional>
#include <vector>

// Aggregated statistics contributed by each PE and combined by the reducer.
struct IdleStats {
    double     sum_idle;           // total idle seconds summed across PEs
    double     max_idle;           // maximum idle seconds across any PE
    long long  sum_union_requests; // total union_request calls summed across PEs
};

// The reducer type handle, registered in initIdleReducer() (FoF.C).
extern CkReduction::reducerType idleReportReducer;

// Declared in Partition.h; accessible here because WorkMonitor.h is compiled
// as part of FoF.C which includes Partition.h (via Paratreet.h) first.
long long fof_get_union_request_count();

// Custom reducer: combine per-PE IdleStats contributions into one aggregate.
static CkReductionMsg* mergeIdleStats(int nMsgs, CkReductionMsg** msgs) {
    IdleStats result = {0.0, 0.0, 0LL};
    for (int i = 0; i < nMsgs; i++) {
        IdleStats* s = reinterpret_cast<IdleStats*>(msgs[i]->getData());
        result.sum_idle          += s->sum_idle;
        result.max_idle           = std::max(result.max_idle, s->max_idle);
        result.sum_union_requests += s->sum_union_requests;
    }
    return CkReductionMsg::buildNew(sizeof(IdleStats), &result);
}

// Convenience alias for the concrete traverser type used by FoF.
using FoFTraverser = TransposedDownTraverser<CentroidData, FoFVisitor>;

// Process-global arrays indexed by CkMyRank().  All PEs in the same process
// share these (SMP shared memory); no ckLocal() on element proxies needed.
// Written only by the PE that owns the slot; read by the relay on any PE.
static constexpr int MAX_LOCAL_PES = 128;
static FoFTraverser*       g_trav_per_rank[MAX_LOCAL_PES]    = {};
static std::atomic<size_t> g_work_per_rank[MAX_LOCAL_PES]    = {};
static int                 g_part_idx_per_rank[MAX_LOCAL_PES]= {};
static size_t              g_trav_idx_per_rank[MAX_LOCAL_PES]= {};
// Per-rank K-trigger state.  Indexed by CkMyRank() so each PE owns its slot
// with no races.  Kept here (not in WorkMonitor) so they work on every
// process — WorkMonitor::ckLocalBranch() is null on non-main processes.
static int                 g_resume_count[MAX_LOCAL_PES]     = {};
static bool                g_help_armed[MAX_LOCAL_PES]       = {};
// Set to true when the first PE on this process wins the K-trigger race.
// Guards against two PEs simultaneously arming two different traversers.
static std::atomic<bool>   g_parallel_triggered{false};

// One instance per PE.  Registers permanent CcdCallOnConditionKeep callbacks
// for CcdPROCESSOR_BEGIN_IDLE and CcdPROCESSOR_END_IDLE at construction time.
// Number of resumeAfterPause calls on a PE before it triggers parallel help.
// K=1 fires on the very first call; increase to let the traversal get started.
static constexpr int PARALLEL_HELP_K = 2500;

struct WorkMonitor : public CBase_WorkMonitor {
    double idle_start  = 0.0;
    double accumulated = 0.0;
    bool   in_idle     = false;

    CProxy_LocalCalcs<CentroidData>     localCalcs;
    CProxy_LocalNodeCalcs<CentroidData> localNodeCalcs;

    WorkMonitor() {
        CcdCallOnConditionKeep(CcdPROCESSOR_BEGIN_IDLE, onBeginIdle, this);
        CcdCallOnConditionKeep(CcdPROCESSOR_END_IDLE,   onEndIdle,   this);
    }
    WorkMonitor(CkMigrateMessage*) {}

    static void onBeginIdle(void* p) {
        auto* self = reinterpret_cast<WorkMonitor*>(p);
        self->idle_start = CkWallTimer();
        self->in_idle    = true;
    }
    static void onEndIdle(void* p) {
        auto* self = reinterpret_cast<WorkMonitor*>(p);
        if (self->in_idle) {
            self->accumulated += CkWallTimer() - self->idle_start;
            self->in_idle = false;
        }
    }

    void resetIdleTime(CkCallback cb) {
        accumulated = 0.0;
        if (in_idle) idle_start = CkWallTimer();
        contribute(0, nullptr, CkReduction::nop, cb);
    }

    void setLocalCalcsProxy(CProxy_LocalCalcs<CentroidData> lc) {
        localCalcs = lc;
    }

    void setLocalNodeCalcsProxy(CProxy_LocalNodeCalcs<CentroidData> lnc) {
        localNodeCalcs = lnc;
    }

    // Called by the relay with do_parallel_help=true when idle imbalance is high.
    // source_rank is the rank (within this process) of the PE whose paused work
    // queue will be shared.  -1 means no live traverser was found; fall through
    // to normal idle reporting.
    // Entry point for the K-trigger path: fan-out from the winning PE.
    // All local PEs receive this; they all call doParallelHelp so the
    // CmiNodeBarrier inside has a matching call from every worker thread.
    void helpSource(int source_rank) {
        doParallelHelp(source_rank);
    }

    void reportIdleTime(CkCallback cb, bool process_tips,
                        bool do_parallel_help, int source_rank) {
        if (process_tips) {
            localCalcs.ckLocalBranch()->doNodeTips();
        }
        // Only enter parallel help if the K-trigger hasn't already fired.
        if (do_parallel_help && source_rank >= 0 && !g_parallel_triggered.load()) {
            doParallelHelp(source_rank);
        }
        double idle = accumulated;
        if (in_idle) idle += CkWallTimer() - idle_start;
        IdleStats s = { idle, idle, fof_get_union_request_count() };
        contribute(sizeof(IdleStats), &s, idleReportReducer, cb);
    }

    // All PEs on this process call this when a parallel phase is triggered.
    // The PE identified by source_rank exposes its paused_curr_nodes; helpers
    // steal chunks via an atomic cursor, run read-only traversal, and collect
    // (vid1, vid2) pairs.  After CmiNodeBarrier each PE applies pairs whose
    // owner chare is local to it.
    void doParallelHelp(int source_rank) {
        LocalNodeCalcs<CentroidData>* lnc = localNodeCalcs.ckLocalBranch();
        std::function<uint64_t(uint64_t)> find_tip = [lnc](uint64_t vid) -> uint64_t {
            bool dummy;
            return lnc ? lnc->localNodeFind(vid, dummy) : vid;
        };
        std::vector<std::pair<uint64_t,uint64_t>> deferred_pairs;

        // Top barrier BEFORE the null check: every PE that received helpSource
        // must arrive here regardless of trav's current value.  A race between
        // helpSource delivery and fof_traversal_done_impl (which nulls the slot)
        // could cause some PEs to see trav==null — if they returned early they
        // would never reach the barrier, deadlocking the PEs that did.
        CmiNodeBarrier();

        FoFTraverser* trav = g_trav_per_rank[source_rank];
        constexpr size_t CHUNK = 32;
        // Declare queue/total at this scope so the source-PE cleanup below can
        // reference them.  When trav is null (slot was cleared by a race with
        // fof_traversal_done_impl) there is no work to steal or erase.
        using QueueType = std::vector<std::pair<Node<CentroidData>*, FoFTraverser::ABType>>;
        QueueType* queue_ptr = trav ? &trav->getPausedWork() : nullptr;
        size_t total = queue_ptr ? queue_ptr->size() : 0;

        if (trav) {
            while (true) {
                size_t my_start = trav->steal_cursor.fetch_add(CHUNK);
                if (my_start >= total) break;
                size_t my_end = std::min(my_start + CHUNK, total);
                for (size_t i = my_start; i < my_end; i++) {
                    auto& [node, active_buckets] = (*queue_ptr)[i];
                    for (int c = 0; c < node->n_children; c++)
                        trav->recurseReadOnly(node->getChild(c), active_buckets,
                                              deferred_pairs, find_tip);
                }
            }
        }

        // Bottom barrier: all PEs reach this unconditionally so the source PE
        // can safely erase the stolen prefix after every reader is done.
        CmiNodeBarrier();

        // Apply phase: mirrors FoFVisitor::leaf() — try both partition endpoints
        // for local delivery (same heuristic), fall back to a remote send.
        for (auto& [v1, v2] : deferred_pairs) {
            if (v1 == v2) continue;
            int pid1 = (int)(v1 >> 32);
            int pid2 = (int)(v2 >> 32);
            int target_idx = ((pid2 < pid1) ^ (pid2 & 1)) ? pid2 : pid1;
            int other_idx  = (target_idx == pid1) ? pid2 : pid1;
            UnionFindLib* lib = libProxy[target_idx].ckLocal();
            if (!lib) lib = libProxy[other_idx].ckLocal();
            if (lib) lib->union_request(v1, v2);
            else     libProxy[target_idx].union_request(v1, v2);
        }

        // Source PE only: erase stolen items and re-trigger traversal.
        if (CkMyRank() == source_rank && trav && queue_ptr) {
            size_t stolen = std::min(trav->steal_cursor.load(), total);
            queue_ptr->erase(queue_ptr->begin(), queue_ptr->begin() + stolen);
            trav->parallel_phase_active = false;
            if (!queue_ptr->empty()) {
                partitionProxy[g_part_idx_per_rank[source_rank]]
                    .resumeAfterPause(g_trav_idx_per_rank[source_rank]);
            }
        }

        // Re-arm: allow the K-trigger to fire again if imbalance recurs later.
        // All PEs reset their own state; the atomic flag is safe to set false here
        // since all PEs are past the barrier and none can re-enter until their
        // current entry method returns.
        g_parallel_triggered.store(false);
        g_help_armed[CkMyRank()] = false;
        g_resume_count[CkMyRank()] = 0;
    }
};

// One instance per process (Charm++ node).  Receives an expedited broadcast
// from IdleMonitorCoordinator and fans out point-to-point expedited sends to
// every WorkMonitor element on this process.
struct WorkMonitorRelay : public CBase_WorkMonitorRelay {
    CProxy_WorkMonitor work_monitor;

    WorkMonitorRelay() {}
    WorkMonitorRelay(CkMigrateMessage*) {}

    void init(CProxy_WorkMonitor wm) { work_monitor = wm; }

    void relayReport(CkCallback cb, bool process_tips, bool do_parallel_help) {
        int first  = CkNodeFirst(CkMyNode());
        int n      = CkNodeSize(CkMyNode());
        int myrank = CkMyRank();

        // Identify the local PE rank (within this process) with the most
        // pending traversal work, using the process-global arrays written by
        // each PE's fof_register_traverser / fof_update_traversal_work hooks.
        int    source_rank = -1;
        size_t max_work    = 0;
        if (do_parallel_help) {
            for (int r = 0; r < n; r++) {
                size_t w = g_work_per_rank[r].load();
                if (w > max_work) { max_work = w; source_rank = r; }
            }
            if (source_rank >= 0) {
                FoFTraverser* trav = g_trav_per_rank[source_rank];
                if (trav) {
                    trav->steal_cursor        = 0;
                    trav->parallel_phase_active = true;
                } else {
                    source_rank = -1;
                }
            }
        }

        for (int r = 1; r < n; r++)
            work_monitor[first + (myrank + r) % n]
                .reportIdleTime(cb, process_tips, do_parallel_help, source_rank);
        work_monitor[first + myrank]
            .reportIdleTime(cb, process_tips, do_parallel_help, source_rank);
    }
};

#endif // PARATREET_WORKMONITOR_H_
