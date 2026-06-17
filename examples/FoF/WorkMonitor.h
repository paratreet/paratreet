#ifndef PARATREET_WORKMONITOR_H_
#define PARATREET_WORKMONITOR_H_

// WorkMonitor.h must be included AFTER FoF.decl.h (for CBase_WorkMonitor).

#include <algorithm>

// Aggregated statistics contributed by each PE and combined by the reducer.
struct IdleStats {
    double     sum_idle;           // total idle seconds summed across PEs
    double     max_idle;           // maximum idle seconds across any PE
    long long  sum_union_requests; // total union_request calls summed across PEs
};

// The reducer type handle, registered in initIdleReducer() (FoF.C).
extern CkReduction::reducerType idleReportReducer;

// Declared in Partition.h; accessible here because WorkMonitor.h is compiled
// as part of FoF.C, which includes Partition.h (via Paratreet.h) first.
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

// One instance per PE.  Registers permanent CcdCallOnConditionKeep callbacks
// for CcdPROCESSOR_BEGIN_IDLE and CcdPROCESSOR_END_IDLE at construction time
// so that the Charm++ scheduler notifies us on every idle/busy transition.
//
// resetIdleTime() zeros the accumulator at the start of each traversal phase
// so that reported times are relative to traversal start, not program start.
//
// reportIdleTime() contributes the total idle time (including any ongoing idle
// period) to a reduction back to IdleMonitorCoordinator.
struct WorkMonitor : public CBase_WorkMonitor {
    double idle_start = 0.0;     // wall time when the current idle period began
    double accumulated = 0.0;    // total idle time since last resetIdleTime()
    bool   in_idle = false;
    CProxy_LocalCalcs<CentroidData> localCalcs;

    WorkMonitor() {
        // Permanent callbacks: fire on every scheduler idle/resume transition.
        CcdCallOnConditionKeep(CcdPROCESSOR_BEGIN_IDLE, onBeginIdle, this);
        CcdCallOnConditionKeep(CcdPROCESSOR_END_IDLE,   onEndIdle,   this);
    }
    WorkMonitor(CkMigrateMessage*) {}

    // Called by the Converse scheduler when this PE's run queue empties.
    // Both callbacks run on the same PE thread as the entry methods, so no
    // locking is needed.
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

    // Broadcast entry: zero the accumulator so idle times are measured from
    // the start of the current traversal phase.  If the PE is already idle
    // when this fires, restart its idle clock from now.
    void resetIdleTime(CkCallback cb) {
        accumulated = 0.0;
        if (in_idle) idle_start = CkWallTimer();
        contribute(0, nullptr, CkReduction::nop, cb);
    }

    void setLocalCalcsProxy(CProxy_LocalCalcs<CentroidData> lc_proxy_) {
        localCalcs = lc_proxy_;
    }

    // Broadcast entry: snapshot the current accumulated idle time (including
    // any ongoing idle period) and contribute it to the reduction.
    void reportIdleTime(CkCallback cb, bool process_tips) {
        if (process_tips) {
            localCalcs.ckLocalBranch()->doNodeTips();
        }
        double idle = accumulated;
        if (in_idle) idle += CkWallTimer() - idle_start;
        IdleStats s = { idle, idle, fof_get_union_request_count() };
        contribute(sizeof(IdleStats), &s, idleReportReducer, cb);
    }
};

// One instance per node.  Receives an expedited broadcast from
// IdleMonitorCoordinator and sends a point-to-point expedited
// reportIdleTime to every WorkMonitor element on this node.
struct WorkMonitorRelay : public CBase_WorkMonitorRelay {
    CProxy_WorkMonitor work_monitor;

    WorkMonitorRelay() {}
    WorkMonitorRelay(CkMigrateMessage*) {}

    void init(CProxy_WorkMonitor wm) {
        work_monitor = wm;
    }

    void relayReport(CkCallback cb, bool process_tips) {
        int first  = CkNodeFirst(CkMyNode());
        int n      = CkNodeSize(CkMyNode());
        int myrank = CkMyRank();
        // Send to all other PEs first, then to self last.
        // The relay PE (self) cannot execute from CsdSchedQueue while it is
        // running this entry method, so it is guaranteed not to be "late".
        // Every other PE receives reportIdleTime while the relay is still
        // looping — before they can drain their queue and fall through to
        // CsdSchedQueue.
        for (int r = 1; r < n; r++)
            work_monitor[first + (myrank + r) % n].reportIdleTime(cb, process_tips);
        work_monitor[first + myrank].reportIdleTime(cb, process_tips);
    }
};

#endif // PARATREET_WORKMONITOR_H_
