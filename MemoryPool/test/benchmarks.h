#pragma once
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <thread>
#include <array>
#include <cstdlib>
#include <ctime>
#include <algorithm>

using namespace std::chrono;

class Timer
{
    high_resolution_clock::time_point start;
public:
    Timer() : start(high_resolution_clock::now()) {}
    double elapsed()
    {
        return duration<double, std::milli>(high_resolution_clock::now() - start).count();
    }
};

template<typename Alloc, typename Dealloc>
void runWarmup(Alloc alloc, Dealloc dealloc)
{
    std::cout << "Warming up...\n";
    std::vector<std::pair<void*, size_t>> ptrs;
    for (int i = 0; i < 1000; ++i)
        for (size_t s : {8, 16, 32, 64, 128, 256, 512})
            ptrs.emplace_back(alloc(s), s);
    for (auto& [p, s] : ptrs) dealloc(p, s);
    std::cout << "Warmup complete.\n\n";
}

template<typename Alloc, typename Dealloc>
void testSmallAllocation(const char* name, Alloc alloc, Dealloc dealloc)
{
    constexpr size_t NUM_ALLOCS = 500000;
    const size_t SIZES[] = { 8, 16, 32, 64, 128, 256 };
    constexpr size_t NUM_SIZES = 6;

    std::cout << "\nTesting small allocations (" << NUM_ALLOCS << " allocations of fixed sizes):\n";

    Timer t;
    std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;
    for (auto& v : sizePtrs) v.reserve(NUM_ALLOCS / NUM_SIZES);

    for (size_t i = 0; i < NUM_ALLOCS; ++i)
    {
        size_t idx = i % NUM_SIZES;
        size_t s = SIZES[idx];
        void* p = alloc(s);
        sizePtrs[idx].push_back({ p, s });

        if (i % 4 == 0) {
            size_t ri = rand() % NUM_SIZES;
            auto& v = sizePtrs[ri];
            if (!v.empty()) { dealloc(v.back().first, v.back().second); v.pop_back(); }
        }
    }
    for (auto& v : sizePtrs)
        for (auto& [p, s] : v) dealloc(p, s);

    std::cout << std::left << std::setw(14) << name
              << std::fixed << std::setprecision(3) << t.elapsed() << " ms\n";
}

template<typename Alloc, typename Dealloc>
void testMultiThreaded(const char* name, Alloc alloc, Dealloc dealloc)
{
    constexpr size_t NUM_THREADS = 6;
    constexpr size_t ALLOCS_PER_THREAD = 100000;

    std::cout << "\nTesting multi-threaded allocations (" << NUM_THREADS
              << " threads, " << ALLOCS_PER_THREAD << " allocations each):\n";

    auto threadBody = [](auto alloc, auto dealloc) {
        srand((unsigned)time(nullptr));
        const size_t SIZES[] = { 8, 16, 32, 64, 128, 256 };
        constexpr size_t NUM_SIZES = 6;

        std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;
        for (auto& v : sizePtrs) v.reserve(ALLOCS_PER_THREAD / NUM_SIZES);

        for (size_t i = 0; i < ALLOCS_PER_THREAD; ++i)
        {
            size_t idx = i % NUM_SIZES;
            size_t s = SIZES[idx];
            sizePtrs[idx].push_back({ alloc(s), s });

            if (i % 100 == 0) {
                size_t ri = rand() % NUM_SIZES;
                auto& v = sizePtrs[ri];
                if (!v.empty()) {
                    size_t n = std::min(v.size() * (20 + rand() % 11) / 100, v.size());
                    for (size_t j = 0; j < n; ++j) {
                        size_t k = rand() % v.size();
                        dealloc(v[k].first, v[k].second);
                        v[k] = v.back(); v.pop_back();
                    }
                }
            }

            if (i % 1000 == 0) {
                std::vector<std::pair<void*, size_t>> burst;
                for (int j = 0; j < 50; ++j) {
                    size_t s = SIZES[rand() % NUM_SIZES];
                    burst.push_back({ alloc(s), s });
                }
                for (auto& [p, s] : burst) dealloc(p, s);
            }
        }
        for (auto& v : sizePtrs)
            for (auto& [p, s] : v) dealloc(p, s);
    };

    Timer t;
    std::vector<std::thread> threads;
    for (size_t i = 0; i < NUM_THREADS; ++i)
        threads.emplace_back([=]() { threadBody(alloc, dealloc); });
    for (auto& th : threads) th.join();

    std::cout << std::left << std::setw(14) << name
              << std::fixed << std::setprecision(3) << t.elapsed() << " ms\n";
}

template<typename Alloc, typename Dealloc>
void testMixedSizes(const char* name, Alloc alloc, Dealloc dealloc)
{
    constexpr size_t NUM_ALLOCS = 500000;
    const size_t SMALL[]  = { 8, 16, 32, 64, 128 };
    const size_t MEDIUM[] = { 256, 384, 512 };
    const size_t LARGE[]  = { 1024, 2048, 4096 };
    constexpr size_t NS = 5, NM = 3, NL = 3, NG = NS + NM + NL;

    std::cout << "\nTesting mixed size allocations (" << NUM_ALLOCS << " allocations):\n";

    Timer t;
    std::array<std::vector<std::pair<void*, size_t>>, NG> groups;
    for (auto& v : groups) v.reserve(NUM_ALLOCS / NG);

    for (size_t i = 0; i < NUM_ALLOCS; ++i)
    {
        int cat = i % 100;
        size_t s, gi;
        if (cat < 60)      { gi = (i/60)%NS;       s = SMALL[gi]; }
        else if (cat < 90) { gi = NS+(i/30)%NM;    s = MEDIUM[gi-NS]; }
        else               { gi = NS+NM+(i/10)%NL; s = LARGE[gi-NS-NM]; }

        groups[gi].push_back({ alloc(s), s });

        if (i % 50 == 0) {
            size_t ri = rand() % NG;
            auto& v = groups[ri];
            if (!v.empty()) {
                size_t n = std::min(v.size() * (20 + rand() % 11) / 100, v.size());
                for (size_t j = 0; j < n; ++j) {
                    size_t k = rand() % v.size();
                    dealloc(v[k].first, v[k].second);
                    v[k] = v.back(); v.pop_back();
                }
            }
        }
    }
    for (auto& v : groups)
        for (auto& [p, s] : v) dealloc(p, s);

    std::cout << std::left << std::setw(14) << name
              << std::fixed << std::setprecision(3) << t.elapsed() << " ms\n";
}
