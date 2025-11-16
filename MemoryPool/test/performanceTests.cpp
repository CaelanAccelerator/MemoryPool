#include "../include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <thread>
#include <array>

using namespace std::chrono;

// Timer utility
class Timer
{
    high_resolution_clock::time_point start;
public:
    Timer() : start(high_resolution_clock::now()) {}

    double elapsed()
    {
        auto end = high_resolution_clock::now();
        return duration_cast<microseconds>(end - start).count() / 1000.0; // milliseconds
    }
};

// Performance test suite
class PerformanceTest
{
private:
    // Aggregated test statistics
    struct TestStats
    {
        double memPoolTime{ 0.0 };
        double systemTime{ 0.0 };
        size_t totalAllocs{ 0 };
        size_t totalBytes{ 0 };
    };

public:
    // 1. System warmup
    static void warmup()
    {
        std::cout << "Warming up memory systems...\n";
        // Use pair to store pointer and its size
        std::vector<std::pair<void*, size_t>> warmupPtrs;

        // Warmup MemoryPool
        for (int i = 0; i < 1000; ++i)
        {
            for (size_t size : {8, 16, 32, 64, 128, 256, 512, 1024}) {
                void* p = MemoryPool::allocate(size);
                warmupPtrs.emplace_back(p, size);  // store pointer and size
            }
        }

        // Release warmup memory
        for (const auto& [ptr, size] : warmupPtrs)
        {
            MemoryPool::deallocate(ptr, size);  // use actual allocated size
        }

        std::cout << "Warmup complete.\n\n";
    }

    // 2. Small object allocation test
    static void testSmallAllocation()
    {
        constexpr size_t NUM_ALLOCS = 100000;
        // Fixed set of small sizes optimized by the pool
        const size_t SIZES[] = { 8, 16, 32, 64, 128, 256 };
        const size_t NUM_SIZES = sizeof(SIZES) / sizeof(SIZES[0]);

        std::cout << "\nTesting small allocations (" << NUM_ALLOCS
            << " allocations of fixed sizes):" << std::endl;

        // MemoryPool test
        {
            Timer t;
            // Store blocks grouped by size
            std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;
            for (auto& ptrs : sizePtrs) {
                ptrs.reserve(NUM_ALLOCS / NUM_SIZES);
            }

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                // Cycle through size classes
                size_t sizeIndex = i % NUM_SIZES;
                size_t size = SIZES[sizeIndex];
                void* ptr = MemoryPool::allocate(size);
                sizePtrs[sizeIndex].push_back({ ptr, size });

                // Simulate real usage: some immediate frees
                if (i % 4 == 0)
                {
                    // Randomly select a size class to free one block
                    size_t releaseIndex = rand() % NUM_SIZES;
                    auto& ptrs = sizePtrs[releaseIndex];

                    if (!ptrs.empty())
                    {
                        MemoryPool::deallocate(ptrs.back().first, ptrs.back().second);
                        ptrs.pop_back();
                    }
                }
            }

            // Cleanup remaining
            for (auto& ptrs : sizePtrs)
            {
                for (const auto& [ptr, size] : ptrs)
                {
                    MemoryPool::deallocate(ptr, size);
                }
            }

            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3)
                << t.elapsed() << " ms" << std::endl;
        }

        // new/delete test
        {
            Timer t;
            std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;
            for (auto& ptrs : sizePtrs) {
                ptrs.reserve(NUM_ALLOCS / NUM_SIZES);
            }

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                size_t sizeIndex = i % NUM_SIZES;
                size_t size = SIZES[sizeIndex];
                void* ptr = new char[size];
                sizePtrs[sizeIndex].push_back({ ptr, size });

                if (i % 4 == 0)
                {
                    size_t releaseIndex = rand() % NUM_SIZES;
                    auto& ptrs = sizePtrs[releaseIndex];

                    if (!ptrs.empty())
                    {
                        delete[] static_cast<char*>(ptrs.back().first);
                        ptrs.pop_back();
                    }
                }
            }

            for (auto& ptrs : sizePtrs)
            {
                for (const auto& [ptr, size] : ptrs)
                {
                    delete[] static_cast<char*>(ptr);
                }
            }

            std::cout << "New/Delete: " << std::fixed << std::setprecision(3)
                << t.elapsed() << " ms" << std::endl;
        }
    }

    // 3. Multi-threaded test
    static void testMultiThreaded()
    {
        constexpr size_t NUM_THREADS = 10;
        constexpr size_t ALLOCS_PER_THREAD = 10000;

        std::cout << "\nTesting multi-threaded allocations (" << NUM_THREADS
            << " threads, " << ALLOCS_PER_THREAD << " allocations each):"
            << std::endl;

        auto threadFunc = [](bool useMemPool)
            {
                std::random_device rd;
                std::mt19937 gen(rd());

                // Fixed size set to better test reuse
                const size_t SIZES[] = { 8, 16, 32, 64, 128, 256 };
                const size_t NUM_SIZES = sizeof(SIZES) / sizeof(SIZES[0]);

                // Per-thread lists grouped by size
                std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;
                for (auto& ptrs : sizePtrs) {
                    ptrs.reserve(ALLOCS_PER_THREAD / NUM_SIZES);
                }

                // Simulate allocation pattern
                for (size_t i = 0; i < ALLOCS_PER_THREAD; ++i)
                {
                    // 1. Allocation phase (prefer ThreadCache)
                    size_t sizeIndex = i % NUM_SIZES;
                    size_t size = SIZES[sizeIndex];
                    void* ptr = useMemPool ? MemoryPool::allocate(size)
                        : new char[size];
                    sizePtrs[sizeIndex].push_back({ ptr, size });

                    // 2. Reuse phase
                    if (i % 100 == 0)
                    {
                        // Random batch free from a size class
                        size_t releaseIndex = rand() % NUM_SIZES;
                        auto& ptrs = sizePtrs[releaseIndex];

                        if (!ptrs.empty())
                        {
                            // Free 20%-30% of blocks of that size
                            size_t releaseCount = ptrs.size() * (20 + (rand() % 11)) / 100;
                            releaseCount = std::min(releaseCount, ptrs.size());

                            for (size_t j = 0; j < releaseCount; ++j)
                            {
                                size_t index = rand() % ptrs.size();
                                if (useMemPool)
                                {
                                    MemoryPool::deallocate(ptrs[index].first, ptrs[index].second);
                                }
                                else
                                {
                                    delete[] static_cast<char*>(ptrs[index].first);
                                }
                                ptrs[index] = ptrs.back();
                                ptrs.pop_back();
                            }
                        }
                    }

                    // 3. Pressure phase: stress CentralCache contention
                    if (i % 1000 == 0)
                    {
                        // Burst allocate then immediately free
                        std::vector<std::pair<void*, size_t>> pressurePtrs;
                        for (int j = 0; j < 50; ++j)
                        {
                            size_t size = SIZES[rand() % NUM_SIZES];
                            void* ptr = useMemPool ? MemoryPool::allocate(size)
                                : new char[size];
                            pressurePtrs.push_back({ ptr, size });
                        }

                        // 立即释放这些内存，测试内存池的回收效率
                        for (const auto& [ptr, size] : pressurePtrs)
                        {
                            if (useMemPool)
                            {
                                MemoryPool::deallocate(ptr, size);
                            }
                            else
                            {
                                delete[] static_cast<char*>(ptr);
                            }
                        }
                    }
                }

                // Cleanup remaining
                for (auto& ptrs : sizePtrs)
                {
                    for (const auto& [ptr, size] : ptrs)
                    {
                        if (useMemPool)
                        {
                            MemoryPool::deallocate(ptr, size);
                        }
                        else
                        {
                            delete[] static_cast<char*>(ptr);
                        }
                    }
                }
            };

        // MemoryPool test
        {
            Timer t;
            std::vector<std::thread> threads;

            for (size_t i = 0; i < NUM_THREADS; ++i)
            {
                threads.emplace_back(threadFunc, true);
            }

            for (auto& thread : threads)
            {
                thread.join();
            }

            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3)
                << t.elapsed() << " ms" << std::endl;
        }

        // new/delete test
        {
            Timer t;
            std::vector<std::thread> threads;

            for (size_t i = 0; i < NUM_THREADS; ++i)
            {
                threads.emplace_back(threadFunc, false);
            }

            for (auto& thread : threads)
            {
                thread.join();
            }

            std::cout << "New/Delete: " << std::fixed << std::setprecision(3)
                << t.elapsed() << " ms" << std::endl;
        }
    }

    // 4. Mixed size test
    static void testMixedSizes()
    {
        constexpr size_t NUM_ALLOCS = 10000;
        // Categorize sizes per allocator design:
        // 1. Small: suited for ThreadCache
        // 2. Medium: suited for CentralCache
        // 3. Large: suited for PageCache
        const size_t SMALL_SIZES[] = { 8, 16, 32, 64, 128 };
        const size_t MEDIUM_SIZES[] = { 256, 384, 512 };
        const size_t LARGE_SIZES[] = { 1024, 2048, 4096 };

        const size_t NUM_SMALL = sizeof(SMALL_SIZES) / sizeof(SMALL_SIZES[0]);
        const size_t NUM_MEDIUM = sizeof(MEDIUM_SIZES) / sizeof(MEDIUM_SIZES[0]);
        const size_t NUM_LARGE = sizeof(LARGE_SIZES) / sizeof(LARGE_SIZES[0]);

        std::cout << "\nTesting mixed size allocations (" << NUM_ALLOCS
            << " allocations with fixed sizes):" << std::endl;

        // MemoryPool test
        {
            Timer t;
            // Group blocks by size category
            std::array<std::vector<std::pair<void*, size_t>>, NUM_SMALL + NUM_MEDIUM + NUM_LARGE> sizePtrs;
            for (auto& ptrs : sizePtrs) {
                ptrs.reserve(NUM_ALLOCS / (NUM_SMALL + NUM_MEDIUM + NUM_LARGE));
            }

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                size_t size;
                int category = i % 100;  // cyclic category

                if (category < 60) {
                    // Small objects
                    size_t index = (i / 60) % NUM_SMALL;
                    size = SMALL_SIZES[index];
                }
                else if (category < 90) {
                    // Medium objects
                    size_t index = (i / 30) % NUM_MEDIUM;
                    size = MEDIUM_SIZES[index];
                }
                else {
                    // Large objects
                    size_t index = (i / 10) % NUM_LARGE;
                    size = LARGE_SIZES[index];
                }

                void* ptr = MemoryPool::allocate(size);
                // 计算在sizePtrs中的索引
                size_t ptrIndex = (category < 60) ? (i / 60) % NUM_SMALL :
                    (category < 90) ? NUM_SMALL + (i / 30) % NUM_MEDIUM :
                    NUM_SMALL + NUM_MEDIUM + (i / 10) % NUM_LARGE;
                sizePtrs[ptrIndex].push_back({ ptr, size });

                // Periodic random frees
                if (i % 50 == 0)
                {
                    // 随机选择一个大小类别进行批量释放
                    size_t releaseIndex = rand() % sizePtrs.size();
                    auto& ptrs = sizePtrs[releaseIndex];

                    if (!ptrs.empty())
                    {
                        // Free 20%-30% of blocks in that size group
                        size_t releaseCount = ptrs.size() * (20 + (rand() % 11)) / 100;
                        releaseCount = std::min(releaseCount, ptrs.size());

                        for (size_t j = 0; j < releaseCount; ++j)
                        {
                            size_t index = rand() % ptrs.size();
                            MemoryPool::deallocate(ptrs[index].first, ptrs[index].second);
                            ptrs[index] = ptrs.back();
                            ptrs.pop_back();
                        }
                    }
                }
            }

            // Cleanup remaining
            for (auto& ptrs : sizePtrs)
            {
                for (const auto& [ptr, size] : ptrs)
                {
                    MemoryPool::deallocate(ptr, size);
                }
            }

            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3)
                << t.elapsed() << " ms" << std::endl;
        }

        // new/delete test
        {
            Timer t;
            std::array<std::vector<std::pair<void*, size_t>>, NUM_SMALL + NUM_MEDIUM + NUM_LARGE> sizePtrs;
            for (auto& ptrs : sizePtrs) {
                ptrs.reserve(NUM_ALLOCS / (NUM_SMALL + NUM_MEDIUM + NUM_LARGE));
            }

            for (size_t i = 0; i < NUM_ALLOCS; ++i)
            {
                size_t size;
                int category = i % 100;

                if (category < 60) {
                    size_t index = (i / 60) % NUM_SMALL;
                    size = SMALL_SIZES[index];
                }
                else if (category < 90) {
                    size_t index = (i / 30) % NUM_MEDIUM;
                    size = MEDIUM_SIZES[index];
                }
                else {
                    size_t index = (i / 10) % NUM_LARGE;
                    size = LARGE_SIZES[index];
                }

                void* ptr = new char[size];
                size_t ptrIndex = (category < 60) ? (i / 60) % NUM_SMALL :
                    (category < 90) ? NUM_SMALL + (i / 30) % NUM_MEDIUM :
                    NUM_SMALL + NUM_MEDIUM + (i / 10) % NUM_LARGE;
                sizePtrs[ptrIndex].push_back({ ptr, size });

                if (i % 50 == 0)
                {
                    size_t releaseIndex = rand() % sizePtrs.size();
                    auto& ptrs = sizePtrs[releaseIndex];

                    if (!ptrs.empty())
                    {
                        size_t releaseCount = ptrs.size() * (20 + (rand() % 11)) / 100;
                        releaseCount = std::min(releaseCount, ptrs.size());

                        for (size_t j = 0; j < releaseCount; ++j)
                        {
                            size_t index = rand() % ptrs.size();
                            delete[] static_cast<char*>(ptrs[index].first);
                            ptrs[index] = ptrs.back();
                            ptrs.pop_back();
                        }
                    }
                }
            }

            for (auto& ptrs : sizePtrs)
            {
                for (const auto& [ptr, size] : ptrs)
                {
                    delete[] static_cast<char*>(ptr);
                }
            }

            std::cout << "New/Delete: " << std::fixed << std::setprecision(3)
                << t.elapsed() << " ms" << std::endl;
        }
    }
};

int main()
{
    std::cout << "Starting performance tests..." << std::endl;

    
    PerformanceTest::warmup();

    
    PerformanceTest::testSmallAllocation();
    /*PerformanceTest::testMultiThreaded();
    PerformanceTest::testMixedSizes();*/

    return 0;
}