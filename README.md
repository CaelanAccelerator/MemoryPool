# MemoryPool

A high-performance **C++20** memory pool with a simple 3-layer design:

1. **ThreadCache** — thread-local allocator for small objects (no locks on the fast path)
2. **CentralCache** — size-class segregated shared lists protected by lightweight **spin locks**
3. **PageCache** — span/page management to reduce fragmentation via span reuse

## Why It Matters
- **1.3× faster** than `new/delete` across all workloads (Ubuntu, Release, 6 threads)
- **3× faster** than `new/delete` on mixed sizes; matches tcmalloc on mixed sizes
- Reduces fragmentation by reusing/recycling spans instead of constantly requesting fresh memory from the OS

## Key Features
- Thread-safe design: per-thread caches + fine-grained spinning per size class
- Size-class batching strategies tuned to reduce lock overhead
- Delayed return + span recycling to avoid memory hoarding
- Simple API:
  - `void* MemoryPool::allocate(size_t size)`
  - `void  MemoryPool::deallocate(void* p, size_t size)`
- Clean C++20 implementation with minimal dependencies

## Project Layout
```
MemoryPool/
  include/              public headers (ThreadCache, CentralCache, PageCache, MemoryPool)
  source/               allocator implementation
  test/
    benchmarks.h        shared benchmark helpers (Timer, test functions)
    bench_mempool.cpp   isolated MemoryPool benchmark
    bench_newdelete.cpp isolated new/delete benchmark
    bench_tcmalloc.cpp  isolated tcmalloc benchmark (requires libgoogle-perftools-dev)
    performanceTests.cpp  combined comparison (legacy)
    unitTests.cpp       correctness tests
dev.py                  build / bench / perf / clean helper
```

## Performance

All results: Ubuntu, Release, GCC, WSL2, 6-core machine.  
Each binary runs in its own process (no cross-allocator heap warm-up).

### Isolated benchmarks — `python dev.py perf -r 10`

| Test | Pool | new/delete | tcmalloc |
|---|---|---|---|
| Small alloc — 500K | ~24 ms | ~32 ms | ~21 ms |
| Multi-threaded — 6T×100K | ~58 ms | ~77 ms | ~62 ms |
| Mixed sizes — 500K | ~14 ms | ~43 ms | ~12 ms |
| **Wall-clock** | **105.6 ms ± 1.5%** | **161.7 ms ± 1.3%** | **108.2 ms ± 1.3%** |

Pool is **1.3× faster** than new/delete on small allocs and **3× faster** on mixed sizes.  
Mixed sizes now match tcmalloc — the tiered size-class scheme (8–2048 B) keeps 1280–2048 B objects  
in the pool rather than falling back to `malloc`.

### Windows baseline (MSVC, x64-Release)

```text

Testing small allocations (500000 allocations of fixed sizes):
Memory Pool: 29.884 ms
New/Delete:  35.203 ms

Testing multi-threaded allocations (6 threads, 100000 allocations each):
Memory Pool: 10.845 ms
New/Delete:  12.767 ms

Testing mixed size allocations (500000 allocations with fixed sizes):
Memory Pool: 13.140 ms
New/Delete:  28.332 ms

```

## Dev Tools

```bash
python dev.py build         # cmake configure + Release build
python dev.py bench         # run each isolated benchmark once
python dev.py perf  [-r N]  # perf stat -r N on each benchmark (default 3)
python dev.py clean         # delete build directory
```
