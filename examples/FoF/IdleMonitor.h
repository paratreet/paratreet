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
    bool active = false;

    IdleMonitorCoordinator() {}
    IdleMonitorCoordinator(CkMigrateMessage*) {}

    void start(CProxy_WorkMonitor p) {
        monitor_proxy = p;
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
        monitor_proxy.reportIdleTime(cb);
    }

    void receiveIdleTimes(CkReductionMsg* msg) {
        double now = CkWallTimer();
        if (!active) { delete msg; return; }

        int n = msg->getSize() / sizeof(IdleReport);
        IdleReport* reports = reinterpret_cast<IdleReport*>(msg->getData());
        /*
        for (int i = 0; i < n; i++) {
            if (reports[i].idle_time >= 0.0) {
                CkPrintf("[IdleMonitor t=%.3f] PE %d idle %.3f s\n",
                         now, reports[i].pe, reports[i].idle_time);
            } else {
                CkPrintf("[IdleMonitor t=%.3f] PE %d not yet started\n",
                         now, reports[i].pe);
            }
        }
        */
        delete msg;

        CcdCallFnAfter(scheduleNext, this, 200.0);
    }

    static void scheduleNext(void* p, double /*walltime*/) {
        reinterpret_cast<IdleMonitorCoordinator*>(p)->triggerCycle();
    }
};

#endif // PARATREET_IDLEMONITOR_H_
