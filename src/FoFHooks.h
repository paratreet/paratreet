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
    extern void (*fof_register_traverser)(void* trav_ptr, int partition_idx, size_t trav_idx);
    // Called from Partition::resumeAfterPause to keep WorkMonitor's work-remaining
    // counter current so the relay can identify the busiest local PE.
    extern void (*fof_update_traversal_work)(size_t remaining);
    // Called at the TOP of Partition::resumeAfterPause (before the early-return
    // check) so WorkMonitor can count calls and trigger parallel help after K.
    extern void (*fof_on_resume)(void* trav_ptr, int partition_idx, size_t trav_idx);
}
#endif

#endif // PARATREET_FOFHOOKS_H_
