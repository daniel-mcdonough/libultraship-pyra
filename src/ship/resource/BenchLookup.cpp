#ifdef SOH_BENCHMARK
#include "ship/resource/BenchLookup.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

namespace Ship {

namespace {

thread_local int tDepth[BENCH_LOOKUP_KINDS] = {};
thread_local uint64_t tLookups[BENCH_LOOKUP_KINDS] = {};
thread_local bool tBinning = false;

// Fixed-size and allocation-free, so binning does not show up in allocshim's counts (§8.31).
// Only the binning thread writes it.
constexpr int kSites = 1024; // power of two
struct Site {
    const char* entry;
    void* caller;
    uint64_t n;
};
Site gSites[kSites];
uint64_t gSitesDropped = 0;

void Bin(const char* entry, void* caller) {
    uint32_t h = (uint32_t)(((uintptr_t)caller >> 1) ^ (uintptr_t)entry) * 2654435761u;
    for (int probe = 0; probe < 64; probe++) {
        Site& s = gSites[(h + (uint32_t)probe) & (kSites - 1)];
        if (s.entry == entry && s.caller == caller) {
            s.n++;
            return;
        }
        if (s.entry == nullptr) {
            s = { entry, caller, 1 };
            return;
        }
    }
    gSitesDropped++;
}

} // namespace

BenchLookupScope::BenchLookupScope(const char* entry, void* caller, int kind) : mKind(kind) {
    if (tDepth[kind]++ == 0) {
        tLookups[kind]++;
        if (tBinning) {
            Bin(entry, caller);
        }
    }
}

BenchLookupScope::~BenchLookupScope() {
    tDepth[mKind]--;
}

uint64_t BenchLookupsThisThread(int kind) {
    return tLookups[kind];
}

void BenchLookupSitesBegin() {
    memset(gSites, 0, sizeof(gSites));
    gSitesDropped = 0;
    tBinning = true;
}

void BenchLookupSitesEnd(const char* path, double periods) {
    tBinning = false;
    Site* used[kSites];
    int n = 0;
    for (auto& s : gSites) {
        if (s.entry != nullptr) {
            used[n++] = &s;
        }
    }
    std::sort(used, used + n, [](const Site* a, const Site* b) { return a->n > b->n; });
    FILE* f = fopen(path, "w");
    if (f == nullptr) {
        return;
    }
    if (periods <= 0.0) {
        periods = 1.0;
    }
    fprintf(f, "# per period (%.0f periods): count entry dso caller-offset; %d sites, %llu dropped\n", periods, n,
            (unsigned long long)gSitesDropped);
    for (int i = 0; i < n; i++) {
        Dl_info info{};
        const char* dso = "?";
        uintptr_t off = (uintptr_t)used[i]->caller;
        if (dladdr(used[i]->caller, &info) != 0) {
            dso = info.dli_fname != nullptr ? info.dli_fname : "?";
            off -= (uintptr_t)info.dli_fbase;
        }
        fprintf(f, "%.2f %s %s 0x%lx\n", (double)used[i]->n / periods, used[i]->entry, dso, (unsigned long)off);
    }
    fclose(f);
}

} // namespace Ship
#endif
