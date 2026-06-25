#ifndef PARATREET_FOFHOOKS_H_
#define PARATREET_FOFHOOKS_H_

#ifdef FOF
// Function pointers set by the FoF application in FoF.C before Driver::run()
// is entered.  Driver::run() calls them to bracket the traversal phase with
// the idle-time monitoring cycle.  Null-checked before calling so the library
// compiles cleanly even without the FoF application registered.
namespace paratreet {
    extern void (*fof_start_idle_monitor)();
    extern void (*fof_stop_idle_monitor)();
    // Called from Partition::startDown to register the active traverser pointer
    // and partition index with the local WorkMonitor.  void* avoids pulling
    // Traverser.h into the core library header.
    // proxy_hint is a void* pointing to the caller's CProxy_Partition<Data> so
    // the FoF app can cache a valid partition proxy on every process without
    // relying on the readonly partitionProxy (which is only updated on process 0).
    extern void (*fof_register_traverser)(void* trav_ptr, int partition_idx, size_t trav_idx, void* proxy_hint);
    // Called from Partition::resumeAfterPause to keep WorkMonitor's work-remaining
    // counter current so the relay can identify the busiest local PE.
    extern void (*fof_update_traversal_work)(size_t remaining);
    // Called at the TOP of Partition::resumeAfterPause (before the early-return
    // check) so WorkMonitor can count calls and trigger parallel help after K.
    extern void (*fof_on_resume)(void* trav_ptr, int partition_idx, size_t trav_idx);
    // Called after resumeAfterPause drains the queue without re-pausing and the
    // traverser reports isFinished() — no more pending work or remote requests.
    // Clears the process-global slot so sibling PEs know this PE is done.
    extern void (*fof_traversal_done)(int partition_idx, size_t trav_idx);
}
#endif

#endif // PARATREET_FOFHOOKS_H_
