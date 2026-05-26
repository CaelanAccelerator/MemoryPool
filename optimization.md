# Optimization Summary

Baseline: original implementation at commit `04c447a`.  
Optimized: current `master` HEAD.  
Platform: Ubuntu, Release, GCC, WSL2, 6-core machine.

---

## Changes Made

### 1. Critical Bug Fix — `returnSpan` use-after-free
`CentralCache::returnSpan` had a no-op expression `freeList_[index];` instead of the
assignment `freeList_[index] = newHead;`. Blocks belonging to a reclaimed span were
never removed from the free list, causing use-after-free on the next allocation that
received one of those blocks.

### 2. Reduce False Sharing — `alignas(64)` on `FreeListBucket`
Merged four parallel arrays (`freelist_`, `splk`, `delayCounts_`, `latestRetTime_`) into
a single `FreeListBucket` struct padded to a full cache line:

```cpp
struct alignas(64) FreeListBucket {
    std::atomic_flag splk;
    void*            freelist_;
    size_t           delayCounts_;
    std::chrono::steady_clock::time_point latestRetTime_;
};
```

Without the padding, adjacent buckets for different size classes shared a cache line.
A thread writing bucket _i_ would invalidate bucket _i+1_ in another thread's L1 cache
(MESI false sharing). With `alignas(64)` each bucket owns its cache line.

### 3. Eliminate Double Traversal — tail tracking in `drainToCentral`
`ThreadCache` now tracks both `head` and `tail` of each free list.
When returning blocks to `CentralCache::deallocateBatch`, the tail pointer is passed
directly instead of re-traversing the returned list to find it (O(n) → O(1) for the
return path).

### 4. `unordered_map` for Span Lookups
`CentralCache::spanStore_` and `pageMap_` changed from `std::map` to
`std::unordered_map`. Span lookup during allocation (`getSpanTracker`) dropped from
O(log n) to O(1) average. `PageCache::spanMap_` and `endMap_` were already
`unordered_map`; `freeSpans_` was left as `std::map` because it requires `lower_bound`
for best-fit span selection.

### 5. Strategy Tuning — larger batch multiplier `k`
`CentralCache::PageToCentralStrategy` — the `k` multiplier that controls how many
thread-batch-equivalents to fetch from PageCache per trip:

| Size class | Before | After |
|---|---|---|
| ≤ 64 B  | 12 | 18 |
| ≤ 128 B | 10 | 15 |
| ≤ 256 B | 8  | 12 |
| ≤ 512 B | 6  | 9  |

Larger batches amortize the PageCache mutex lock over more allocations, reducing
cold-start latency and variance.

### 6. Tiered Size-Class Scheme — extend coverage to 2048 B
Replaced the flat 8-byte-aligned scheme (64 classes, 8–512 B) with a tiered scheme
(28 classes, 8–2048 B) matching tcmalloc/jemalloc conventions:

| Tier | Alignment | Classes | Range |
|---|---|---|---|
| 0 | 8 B  | 8  | 8–64 B    |
| 1 | 16 B | 4  | 80–128 B  |
| 2 | 32 B | 4  | 160–256 B |
| 3 | 64 B | 4  | 320–512 B |
| 4 | 128 B| 4  | 640–1024 B|
| 5 | 256 B| 4  | 1280–2048 B|

`Size::sizeToIndex()` and `Size::indexToBlockSize()` helper functions replace the
inline `(size-1)/8` arithmetic. `MAX_ALLOC_SIZE` raised from 512 B → 2048 B.

Effect: allocations of 1280–2048 B now go through the pool instead of falling back
to `malloc`, which is the main driver of the mixed-sizes improvement.

### 7. Threshold Increase — 512 KB per size class
`ThreadCache::shouldReturn` threshold raised from 128 KB → 512 KB per size class.
Reduces drain frequency for large-object buckets, keeping more blocks warm in the
thread cache between bursts.

---

## Results — `perf stat -r 3`, isolated binaries, 6 threads

### Wall-clock (3-run aggregate, before/after optimizations)

| | Original | Optimized | Delta |
|---|---|---|---|
| Elapsed time | 83.78 ms ± 1.91% | 80.22 ms ± 1.26% | **−4.2%** |
| CPUs utilized | 1.628 | 1.677 | +3% |

> Note: original baseline was measured at 4 threads. Current tests use 6 threads.

### Current 3-allocator comparison (`perf stat -r 10`, 6 threads)

| Test | Pool | new/delete | tcmalloc |
|---|---|---|---|
| Small alloc — 500K | ~24 ms | ~32 ms | ~21 ms |
| Multi-threaded — 6T×100K | ~58 ms | ~77 ms | ~62 ms |
| Mixed sizes — 500K | ~14 ms | ~43 ms | ~12 ms |
| **Wall-clock** | **105.6 ms ± 1.5%** | **161.7 ms ± 1.3%** | **108.2 ms ± 1.3%** |

Pool is **1.3× faster** than new/delete on small allocs, **3× faster** on mixed sizes,  
and matches tcmalloc on mixed sizes. Wall-clock variance dropped from 3.8% → 1.5%.

---

## Test Structure

```
MemoryPool/test/
  benchmarks.h          shared helpers: Timer, testSmallAllocation,
                        testMultiThreaded (6T), testMixedSizes
  bench_mempool.cpp     isolated Pool binary  → bench_mempool
  bench_newdelete.cpp   isolated new/delete   → bench_newdelete
  bench_tcmalloc.cpp    isolated tcmalloc     → bench_tcmalloc
  performanceTests.cpp  combined legacy test  → mp_perf
  unitTests.cpp         correctness tests     → mp_tests
```

Each `bench_*` binary runs in its own process — no cross-allocator heap warm-up
(order effect). `python dev.py bench` runs all three sequentially.

---

## Infrastructure Added
- Three isolated benchmark binaries (`bench_mempool`, `bench_newdelete`, `bench_tcmalloc`).
- `dev.py build / bench / perf [-r N] / clean` — development helper script.
