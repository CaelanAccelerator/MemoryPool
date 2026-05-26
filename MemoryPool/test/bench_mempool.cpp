#include "benchmarks.h"
#include "../include/MemoryPool.h"

int main()
{
    auto alloc   = MemoryPool::allocate;
    auto dealloc = MemoryPool::deallocate;

    runWarmup(alloc, dealloc);
    testSmallAllocation("Memory Pool:", alloc, dealloc);
    testMultiThreaded  ("Memory Pool:", alloc, dealloc);
    testMixedSizes     ("Memory Pool:", alloc, dealloc);
}
