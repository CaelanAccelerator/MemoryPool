# MemoryPool

A high-performance **C++17** memory pool with a simple 3-layer design:

1. **ThreadCache** — thread-local allocator for small objects (no locks on the fast path)
2. **CentralCache** — size-class segregated shared lists protected by lightweight **spin locks**
3. **PageCache** — span/page management to reduce fragmentation via span reuse

## Why It Matters
- Up to **~2× faster** than raw `new/delete` on small & mixed allocations (see `performanceTests.cpp`)
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

## Performance Notes
- Configuration: **x64-Release**
- Workloads: `MemoryPool/test/performanceTests.cpp`

### Sample Results (Release)
```text
Starting performance tests...
Warming up memory systems...
Warmup complete.

Testing small allocations (500000 allocations of fixed sizes):
Memory Pool: 16.792 ms
New/Delete: 34.123 ms

Testing multi-threaded allocations (8 threads, 100000 allocations each):
Memory Pool: 13.721 ms
New/Delete: 15.846 ms

Testing mixed size allocations (500000 allocations with fixed sizes):
Memory Pool: 11.465 ms
New/Delete: 27.311 ms
