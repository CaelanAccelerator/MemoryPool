#include "../include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <thread>
#include <array>

#ifdef HAVE_TCMALLOC
#include <gperftools/tcmalloc.h>
#endif

using namespace std::chrono;

class Timer
{
    high_resolution_clock::time_point start;
public:
    Timer() : start(high_resolution_clock::now()) {}

    double elapsed()
    {
        auto end = high_resolution_clock::now();
        return duration<double, std::milli>(end - start).count();
    }
};

class PerformanceTest
{
public:
    static void warmup()
    {
        std::cout << "Warming up memory systems...\n";
        std::vector<std::pair<void*, size_t>> warmupPtrs;

        for (int i = 0; i < 1000; ++i)
        {
            for (size_t size : {8, 16, 32, 64, 128, 256, 512}) {
                void* p = MemoryPool::allocate(size);
                warmupPtrs.emplace_back(p, size);
            }
        }

        for (const auto& [ptr, size] : warmupPtrs)
            MemoryPool::deallocate(ptr, size);

        std::cout << "Warmup complete.\n\n";
    }

    static void testSmallAllocation()
    {
        constexpr size_t NUM_ALLOCS = 500000;
        const size_t SIZES[] = { 8, 16, 32, 64, 128, 256 };
        constexpr size_t NUM_SIZES = 6;

        std::cout << "\nTesting small allocations (" << NUM_ALLOCS
            << " allocations of fixed sizes):\n";

        auto run = [&](const char* name, auto alloc, auto dealloc) {
            Timer t;
            std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;
            for (auto& ptrs : sizePtrs) ptrs.reserve(NUM_ALLOCS / NUM_SIZES);

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                size_t sizeIndex = i % NUM_SIZES;
                size_t size = SIZES[sizeIndex];
                void* ptr = alloc(size);
                sizePtrs[sizeIndex].push_back({ ptr, size });

                if (i % 4 == 0) {
                    size_t releaseIndex = rand() % NUM_SIZES;
                    auto& ptrs = sizePtrs[releaseIndex];
                    if (!ptrs.empty()) {
                        dealloc(ptrs.back().first, ptrs.back().second);
                        ptrs.pop_back();
                    }
                }
            }

            for (auto& ptrs : sizePtrs)
                for (const auto& [ptr, size] : ptrs)
                    dealloc(ptr, size);

            std::cout << std::left << std::setw(13) << name
                << std::fixed << std::setprecision(3) << t.elapsed() << " ms\n";
        };

        run("Memory Pool:", MemoryPool::allocate, MemoryPool::deallocate);
        run("New/Delete: ",
            [](size_t s) -> void* { return new char[s]; },
            [](void* p, size_t) { delete[] static_cast<char*>(p); });
#ifdef HAVE_TCMALLOC
        run("TCMalloc:   ", tc_malloc, [](void* p, size_t) { tc_free(p); });
#endif
    }

    static void testMultiThreaded()
    {
        constexpr size_t NUM_THREADS = 4;
        constexpr size_t ALLOCS_PER_THREAD = 100000;

        std::cout << "\nTesting multi-threaded allocations (" << NUM_THREADS
            << " threads, " << ALLOCS_PER_THREAD << " allocations each):\n";

        auto threadBody = [](auto alloc, auto dealloc) {
            srand((unsigned)time(nullptr));
            const size_t SIZES[] = { 8, 16, 32, 64, 128, 256 };
            constexpr size_t NUM_SIZES = 6;

            std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;
            for (auto& ptrs : sizePtrs) ptrs.reserve(ALLOCS_PER_THREAD / NUM_SIZES);

            for (size_t i = 0; i < ALLOCS_PER_THREAD; ++i)
            {
                size_t sizeIndex = i % NUM_SIZES;
                size_t size = SIZES[sizeIndex];
                void* ptr = alloc(size);
                sizePtrs[sizeIndex].push_back({ ptr, size });

                if (i % 100 == 0) {
                    size_t releaseIndex = rand() % NUM_SIZES;
                    auto& ptrs = sizePtrs[releaseIndex];
                    if (!ptrs.empty()) {
                        size_t releaseCount = ptrs.size() * (20 + (rand() % 11)) / 100;
                        releaseCount = std::min(releaseCount, ptrs.size());
                        for (size_t j = 0; j < releaseCount; ++j) {
                            size_t index = rand() % ptrs.size();
                            dealloc(ptrs[index].first, ptrs[index].second);
                            ptrs[index] = ptrs.back();
                            ptrs.pop_back();
                        }
                    }
                }

                // Stress CentralCache contention
                if (i % 1000 == 0) {
                    std::vector<std::pair<void*, size_t>> pressurePtrs;
                    for (int j = 0; j < 50; ++j) {
                        size_t sz = SIZES[rand() % NUM_SIZES];
                        pressurePtrs.push_back({ alloc(sz), sz });
                    }
                    for (const auto& [p, s] : pressurePtrs)
                        dealloc(p, s);
                }
            }

            for (auto& ptrs : sizePtrs)
                for (const auto& [ptr, size] : ptrs)
                    dealloc(ptr, size);
        };

        auto run = [&](const char* name, auto alloc, auto dealloc) {
            Timer t;
            std::vector<std::thread> threads;
            for (size_t i = 0; i < NUM_THREADS; ++i)
                threads.emplace_back([=]() { threadBody(alloc, dealloc); });
            for (auto& th : threads) th.join();
            std::cout << std::left << std::setw(13) << name
                << std::fixed << std::setprecision(3) << t.elapsed() << " ms\n";
        };

        run("Memory Pool:", MemoryPool::allocate, MemoryPool::deallocate);
        run("New/Delete: ",
            [](size_t s) -> void* { return new char[s]; },
            [](void* p, size_t) { delete[] static_cast<char*>(p); });
#ifdef HAVE_TCMALLOC
        run("TCMalloc:   ", tc_malloc, [](void* p, size_t) { tc_free(p); });
#endif
    }

    static void testMixedSizes()
    {
        srand((unsigned)time(nullptr));
        constexpr size_t NUM_ALLOCS = 500000;
        const size_t SMALL_SIZES[]  = { 8, 16, 32, 64, 128 };
        const size_t MEDIUM_SIZES[] = { 256, 384, 512 };
        const size_t LARGE_SIZES[]  = { 1024, 2048, 4096 };
        constexpr size_t NUM_SMALL  = 5;
        constexpr size_t NUM_MEDIUM = 3;
        constexpr size_t NUM_LARGE  = 3;
        constexpr size_t NUM_GROUPS = NUM_SMALL + NUM_MEDIUM + NUM_LARGE;

        std::cout << "\nTesting mixed size allocations (" << NUM_ALLOCS
            << " allocations with fixed sizes):\n";

        auto run = [&](const char* name, auto alloc, auto dealloc) {
            Timer t;
            std::array<std::vector<std::pair<void*, size_t>>, NUM_GROUPS> sizePtrs;
            for (auto& ptrs : sizePtrs) ptrs.reserve(NUM_ALLOCS / NUM_GROUPS);

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                size_t size;
                size_t ptrIndex;
                int category = i % 100;

                if (category < 60) {
                    size_t index = (i / 60) % NUM_SMALL;
                    size = SMALL_SIZES[index];
                    ptrIndex = index;
                } else if (category < 90) {
                    size_t index = (i / 30) % NUM_MEDIUM;
                    size = MEDIUM_SIZES[index];
                    ptrIndex = NUM_SMALL + index;
                } else {
                    size_t index = (i / 10) % NUM_LARGE;
                    size = LARGE_SIZES[index];
                    ptrIndex = NUM_SMALL + NUM_MEDIUM + index;
                }

                void* ptr = alloc(size);
                sizePtrs[ptrIndex].push_back({ ptr, size });

                if (i % 50 == 0) {
                    size_t releaseIndex = rand() % NUM_GROUPS;
                    auto& ptrs = sizePtrs[releaseIndex];
                    if (!ptrs.empty()) {
                        size_t releaseCount = ptrs.size() * (20 + (rand() % 11)) / 100;
                        releaseCount = std::min(releaseCount, ptrs.size());
                        for (size_t j = 0; j < releaseCount; ++j) {
                            size_t index = rand() % ptrs.size();
                            dealloc(ptrs[index].first, ptrs[index].second);
                            ptrs[index] = ptrs.back();
                            ptrs.pop_back();
                        }
                    }
                }
            }

            for (auto& ptrs : sizePtrs)
                for (const auto& [ptr, size] : ptrs)
                    dealloc(ptr, size);

            std::cout << std::left << std::setw(13) << name
                << std::fixed << std::setprecision(3) << t.elapsed() << " ms\n";
        };

        run("Memory Pool:", MemoryPool::allocate, MemoryPool::deallocate);
        run("New/Delete: ",
            [](size_t s) -> void* { return new char[s]; },
            [](void* p, size_t) { delete[] static_cast<char*>(p); });
#ifdef HAVE_TCMALLOC
        run("TCMalloc:   ", tc_malloc, [](void* p, size_t) { tc_free(p); });
#endif
    }
};

int main()
{
    std::cout << "Starting performance tests...\n";

    PerformanceTest::warmup();
    PerformanceTest::testSmallAllocation();
    PerformanceTest::testMultiThreaded();
    PerformanceTest::testMixedSizes();

    return 0;
}
