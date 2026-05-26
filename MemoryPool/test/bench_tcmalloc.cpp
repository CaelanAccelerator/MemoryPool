#include "benchmarks.h"

#ifdef HAVE_TCMALLOC
#include <gperftools/tcmalloc.h>
#endif

int main()
{
#ifndef HAVE_TCMALLOC
    std::cerr << "Built without tcmalloc. Reconfigure with libgoogle-perftools-dev installed.\n";
    return 1;
#else
    auto alloc   = tc_malloc;
    auto dealloc = [](void* p, size_t) { tc_free(p); };

    runWarmup(alloc, dealloc);
    testSmallAllocation("TCMalloc:", alloc, dealloc);
    testMultiThreaded  ("TCMalloc:", alloc, dealloc);
    testMixedSizes     ("TCMalloc:", alloc, dealloc);
#endif
}
