#include "../include/ThreadCache.h"   
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
#include <cstring>
#include <random>
#include <algorithm>
#include <atomic>
#include <cstdint>


static inline void* MP_allocate(size_t size) {
    return ThreadCache::getInstance().allocate(size);
}
static inline void MP_deallocate(void* p, size_t size) {
    ThreadCache::getInstance().deallocate(p, size);
}

// Basic sanity for small / medium / large allocations including path > MAX_SIZE.       
void testBasicAllocation() {
    std::cout << "Running basic allocation test..." << std::endl;

    void* ptr1 = MP_allocate(8);
    assert(ptr1 != nullptr);
    MP_deallocate(ptr1, 8);

    void* ptr2 = MP_allocate(1024);
    assert(ptr2 != nullptr);
    MP_deallocate(ptr2, 1024);

    const size_t big = 1024 * 1024;
    void* ptr3 = MP_allocate(big);
    assert(ptr3 != nullptr);
    MP_deallocate(ptr3, big);

    std::cout << "Basic allocation test passed!" << std::endl;
}

void testMemoryWriting() {
    std::cout << "Running memory writing test..." << std::endl;

    const size_t size = 128;
    char* p = static_cast<char*>(MP_allocate(size));
    assert(p != nullptr);

    for (size_t i = 0; i < size; ++i) p[i] = static_cast<char>(i % 256);
    for (size_t i = 0; i < size; ++i) assert(p[i] == static_cast<char>(i % 256));

    MP_deallocate(p, size);
    std::cout << "Memory writing test passed!" << std::endl;
}

void testMultiThreading() {
    std::cout << "Running multi-threading test..." << std::endl;

    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 1000;
    std::atomic<bool> has_error{ false };

    auto threadFunc = [&]() {
        try {
            std::vector<std::pair<void*, size_t>> allocations;
            allocations.reserve(ALLOCS_PER_THREAD);

            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<int> smallK(1, 256);  
            std::bernoulli_distribution coin(0.5);

            for (int i = 0; i < ALLOCS_PER_THREAD && !has_error.load(); ++i) {
                size_t sz = static_cast<size_t>(smallK(rng)) * 8; 
                void* ptr = MP_allocate(sz);
                if (!ptr) { has_error = true; break; }
                allocations.push_back({ ptr, sz });

                if (coin(rng) && !allocations.empty()) {
                    size_t idx = static_cast<size_t>(rng()) % allocations.size();
                    MP_deallocate(allocations[idx].first, allocations[idx].second);
                    allocations.erase(allocations.begin() + idx);
                }
            }
            for (auto& kv : allocations) MP_deallocate(kv.first, kv.second);
        }
        catch (...) {
            has_error = true;
        }
        };

    std::vector<std::thread> ths;
    ths.reserve(NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; ++i) ths.emplace_back(threadFunc);
    for (auto& t : ths) t.join();

    assert(!has_error.load());
    std::cout << "Multi-threading test passed!" << std::endl;
}

void testEdgeCases() {
    std::cout << "Running edge cases test..." << std::endl;

    void* p0 = MP_allocate(0);
    if (p0) MP_deallocate(p0, 0);

    void* p1 = MP_allocate(1);
    if (p1) {
        assert((reinterpret_cast<uintptr_t>(p1) & (8 - 1)) == 0);
        MP_deallocate(p1, 1);
    }

    const size_t nearMaxSmall = 256 * 1024;
    void* p2 = MP_allocate(nearMaxSmall);
    if (p2) MP_deallocate(p2, nearMaxSmall);

    const size_t overMaxSmall = 1024 * 1024;
    void* p3 = MP_allocate(overMaxSmall);
    assert(p3 != nullptr);
    MP_deallocate(p3, overMaxSmall);

    std::cout << "Edge cases test passed!" << std::endl;
}

void testStress() {
    std::cout << "Running stress test..." << std::endl;

    const int NUM_ITER = 10000;
    std::vector<std::pair<void*, size_t>> allocations;
    allocations.reserve(NUM_ITER);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> smallK(1, 1024); 

    for (int i = 0; i < NUM_ITER; ++i) {
        size_t sz = static_cast<size_t>(smallK(rng)) * 8;
        void* p = MP_allocate(sz);
        assert(p != nullptr);
        allocations.push_back({ p, sz });
    }

    std::shuffle(allocations.begin(), allocations.end(), rng);
    for (auto& kv : allocations) MP_deallocate(kv.first, kv.second);

    std::cout << "Stress test passed!" << std::endl;
}

int main() {
    try {
        std::cout << "Starting memory pool tests..." << std::endl;

        testBasicAllocation();
        testMemoryWriting();
        testMultiThreading();
        testEdgeCases();
        testStress();

        std::cout << "All tests passed successfully!" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
