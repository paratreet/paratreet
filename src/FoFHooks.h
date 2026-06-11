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
}
#endif

#endif // PARATREET_FOFHOOKS_H_
