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
    bool do_process_tips_next = false; //when to do doNodeTips
    bool process_tips_done = false;

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
        monitor_proxy.reportIdleTime(cb, do_process_tips_next);
        if(do_process_tips_next) {
            printf("Process tips was triggered\n");
            do_process_tips_next = false;
            process_tips_done = true;
        }
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

        //loop to calculate average idle time across all PEs
        if(!process_tips_done) {
            double total_idle = 0.0;
            int count = 0;
            double max_idle = 0.0;
            for (int i = 0; i < n; i++) {
                if (reports[i].idle_time >= 0.0) {
                    total_idle += reports[i].idle_time;
                    count++;
                }
                if (reports[i].idle_time > max_idle) {
                    max_idle = reports[i].idle_time;
                }
            }
            double avg_idle = (count > 0) ? total_idle / count : 0.0;
            //if the max idle time is 3x the average, set do_process_tips_next to true
            printf("[IdleMonitor t=%.3f] Average and max idle time across %d PEs: %.3f s, %.3f s\n", now, count, avg_idle, max_idle);
            if (avg_idle > 0.05 && max_idle > 3 * avg_idle) {
                do_process_tips_next = true;
            }
        }

        CcdCallFnAfter(scheduleNext, this, 200.0);
    }

    static void scheduleNext(void* p, double /*walltime*/) {
        reinterpret_cast<IdleMonitorCoordinator*>(p)->triggerCycle();
    }
};

#endif // PARATREET_IDLEMONITOR_H_
