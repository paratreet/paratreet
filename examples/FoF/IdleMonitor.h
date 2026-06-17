#ifndef PARATREET_IDLEMONITOR_H_
#define PARATREET_IDLEMONITOR_H_

// IdleMonitor.h must be included AFTER WorkMonitor.h and FoF.decl.h.

#include "WorkMonitor.h"

// Singleton coordinator chare (lives on one PE, typically PE 0).  Drives the
// periodic broadcast → reduction cycle independently of Driver, which is
// blocked inside CkWaitQD() during traversal and cannot itself receive
// reduction results.
//
// Lifecycle:
//   Driver calls fof_start_idle_monitor() → start(workMonitor)
//     → triggerCycle() sends reportIdleTime broadcast to WorkMonitor group
//     → receiveIdleTimes() logs results, schedules next cycle via CcdCallFnAfter
//   Driver calls fof_stop_idle_monitor() → stop() sets active=false
//     → next receiveIdleTimes() or triggerCycle() sees !active and returns
struct IdleMonitorCoordinator : public CBase_IdleMonitorCoordinator {
    CProxy_WorkMonitor monitor_proxy;
    CProxy_WorkMonitorRelay relay_proxy;
    bool active = false;
    bool do_process_tips_next = false; //when to do doNodeTips
    bool process_tips_done = false;

    IdleMonitorCoordinator() {}
    IdleMonitorCoordinator(CkMigrateMessage*) {}

    void start(CProxy_WorkMonitor p, CProxy_WorkMonitorRelay relay) {
        monitor_proxy = p;
        relay_proxy = relay;
        active = true;
        triggerCycle();
    }

    void stop() {
        active = false;
    }

    // Broadcast to all WorkMonitor elements; their contributions reduce back
    // here via receiveIdleTimes.  Not declared as an entry method: called only
    // from start() and from the CcdCallFnAfter callback on this PE.
    void triggerCycle() {
        if (!active) return;
        CkCallback cb(CkIndex_IdleMonitorCoordinator::receiveIdleTimes(nullptr),
                      this->thisProxy);
        relay_proxy.relayReport(cb, do_process_tips_next);
        if(do_process_tips_next) {
            printf("Process tips was triggered\n");
            do_process_tips_next = false;
            process_tips_done = true;
        }
    }

    void receiveIdleTimes(CkReductionMsg* msg) {
        double now = CkWallTimer();
        if (!active) { delete msg; return; }

        IdleStats* stats = reinterpret_cast<IdleStats*>(msg->getData());
        double    sum_idle    = stats->sum_idle;
        double    max_idle    = stats->max_idle;
        long long sum_unions  = stats->sum_union_requests;
        delete msg;

        if (!process_tips_done) {
            int n_pes = CkNumPes();
            double avg_idle = (n_pes > 0) ? sum_idle / n_pes : 0.0;
            printf("[IdleMonitor t=%.3f] idle sum=%.3f s avg=%.3f s max=%.3f s  union_requests=%lld\n",
                   now, sum_idle, avg_idle, max_idle, sum_unions);
            if (avg_idle > 0.05 && max_idle > 1.5 * avg_idle) {
                do_process_tips_next = true;
            }
        }
        else
        {
            //print union_requests after processing tips
            printf("[IdleMonitor t=%.3f] union_requests after processing tips=%lld\n", now, sum_unions);
        }

        CcdCallFnAfter(scheduleNext, this, 200.0);
    }

    static void scheduleNext(void* p, double /*walltime*/) {
        reinterpret_cast<IdleMonitorCoordinator*>(p)->triggerCycle();
    }
};

#endif // PARATREET_IDLEMONITOR_H_
