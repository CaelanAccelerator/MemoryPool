#include "benchmarks.h"

int main()
{
    auto alloc   = [](size_t s) -> void* { return new char[s]; };
    auto dealloc = [](void* p, size_t)   { delete[] static_cast<char*>(p); };

    runWarmup(alloc, dealloc);
    testSmallAllocation("New/Delete:", alloc, dealloc);
    testMultiThreaded  ("New/Delete:", alloc, dealloc);
    testMixedSizes     ("New/Delete:", alloc, dealloc);
}
