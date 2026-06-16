#ifndef PARATREET_WORKMONITOR_H_
#define PARATREET_WORKMONITOR_H_

// WorkMonitor.h must be included AFTER FoF.decl.h (for CBase_WorkMonitor).

#include <cstring>

// Per-PE report contributed to each monitoring reduction.
struct IdleReport {
    int    pe;
    double idle_time;  // seconds idle since the last resetIdleTime() call
};

// The reducer type handle, registered in initIdleReducer() (FoF.C).
extern CkReduction::reducerType idleReportReducer;

// Custom reducer: concatenate per-PE IdleReport structs into a single flat
// array.  The coordinator receives the full array and logs each entry.
static CkReductionMsg* mergeIdleReports(int nMsgs, CkReductionMsg** msgs) {
    int total = 0;
    for (int i = 0; i < nMsgs; i++)
        total += msgs[i]->getSize() / sizeof(IdleReport);
    CkReductionMsg* out = CkReductionMsg::buildNew(total * sizeof(IdleReport), nullptr);
    IdleReport* dst = reinterpret_cast<IdleReport*>(out->getData());
    int k = 0;
    for (int i = 0; i < nMsgs; i++) {
        int n = msgs[i]->getSize() / sizeof(IdleReport);
        std::memcpy(dst + k, msgs[i]->getData(), n * sizeof(IdleReport));
        k += n;
    }
    return out;
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
        if(process_tips){
            localCalcs.ckLocalBranch()->doNodeTips();
        }
        double total = accumulated;
        if (in_idle) total += CkWallTimer() - idle_start;
        IdleReport r = { CkMyPe(), total };
        contribute(sizeof(IdleReport), &r, idleReportReducer, cb);
    }
};

#endif // PARATREET_WORKMONITOR_H_
