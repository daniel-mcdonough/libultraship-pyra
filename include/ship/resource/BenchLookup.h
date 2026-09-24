#pragma once

// Benchmark builds only (report 8 §8.31): resource lookups by name or CRC, counted once each at
// their outermost entry into the resource manager, and binned by entry point and caller.
//
// A lookup that hits the cache never reaches LoadResourceProcess, so gBenchResourceLoads cannot
// see it; this can. A scope nests: GetResourceRawPointer(name) inside ResourceGetDataByName is one
// lookup, credited to the bridge function and to whoever called it.

#ifdef SOH_BENCHMARK
#include <cstdint>

namespace Ship {

// What is being looked up. Each kind nests and is counted on its own.
enum BenchLookupKind { BENCH_LOOKUP_RESOURCE, BENCH_LOOKUP_CVAR, BENCH_LOOKUP_KINDS };

struct BenchLookupScope {
    BenchLookupScope(const char* entry, void* caller, int kind = BENCH_LOOKUP_RESOURCE);
    ~BenchLookupScope();
    int mKind;
};

// Outermost lookups of one kind made by the calling thread, cumulative.
uint64_t BenchLookupsThisThread(int kind = BENCH_LOOKUP_RESOURCE);
// Bin the calling thread's lookups by (entry, caller) from now on; stop and write the table to
// path, largest first, per period.
void BenchLookupSitesBegin();
void BenchLookupSitesEnd(const char* path, double periods);

} // namespace Ship

#define BENCH_LOOKUP_SCOPE(entry) \
    const Ship::BenchLookupScope benchLookupScope_((entry), __builtin_return_address(0))
#define BENCH_CVAR_SCOPE(entry) \
    const Ship::BenchLookupScope benchLookupScope_((entry), __builtin_return_address(0), Ship::BENCH_LOOKUP_CVAR)
#else
#define BENCH_LOOKUP_SCOPE(entry) ((void)0)
#define BENCH_CVAR_SCOPE(entry) ((void)0)
#endif
