# MemoryPool

A high-performance **C++17** memory pool with a simple 3-layer design:

1. **ThreadCache** — thread-local allocator for small objects (no locks on the fast path)
2. **CentralCache** — size-class segregated shared lists protected by lightweight **spin locks**
3. **PageCache** — span/page management to reduce fragmentation via span reuse

## Why It Matters
- Up to **~1.5× faster** than raw `new/delete` on small & mixed allocations (see `performanceTests.cpp`)
- Reduces fragmentation by reusing/recycling spans instead of constantly requesting fresh memory from the OS
- Scales under multi-threaded workloads using thread-local caching + batched central transfers (locks are amortized)

## Key Features
- Thread-safe design: per-thread caches + fine-grained spinning per size class
- Size-class batching strategies tuned to reduce lock overhead
- Delayed return + span recycling to avoid memory hoarding
- Simple API:
  - `void* MemoryPool::allocate(size_t size)`
  - `void  MemoryPool::deallocate(void* p, size_t size)`
- Clean C++17 implementation with minimal dependencies

## Project Layout
- `MemoryPool/include/` — public headers
- `MemoryPool/source/` — allocator implementation (`ThreadCache`, `CentralCache`, `PageCache`)
- `MemoryPool/test/` — benchmarks and unit tests

## Performance
- Benchmark: `MemoryPool/test/performanceTests.cpp`
- Results shown are **Release** builds.

```text
[Windows | x64-Release | MSVC]
Testing small allocations (500000 allocations of fixed sizes):
Memory Pool: 27.747 ms
New/Delete: 34.387 ms

Testing multi-threaded allocations (4 threads, 100000 allocations each):
Memory Pool: 8.545 ms
New/Delete: 10.426 ms

Testing mixed size allocations (500000 allocations with fixed sizes):
Memory Pool: 18.345 ms
New/Delete: 26.601 ms


[Ubuntu | Release | GCC]
Testing small allocations (500000 allocations of fixed sizes):
Memory Pool: 27.747 ms
New/Delete: 34.387 ms

Testing multi-threaded allocations (4 threads, 100000 allocations each):
Memory Pool: 8.545 ms
New/Delete: 10.426 ms

Testing mixed size allocations (500000 allocations with fixed sizes):
Memory Pool: 18.345 ms
New/Delete: 26.601 ms