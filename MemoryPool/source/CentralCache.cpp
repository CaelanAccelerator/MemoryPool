#include "CentralCache.h"
#include <thread>
#include <PageCache.h>
#include <unordered_map>
#include "Size.h"
#include <cstddef>
#include "SpinLockGuard.h"
using std::size_t;

const std::chrono::milliseconds CentralCache::MAX_DELAY_DURATION{ 1000 };
CentralCache::CentralCache()
{
    for (auto& ptr : freeList_)
        ptr = nullptr;

    for (auto& lock : locks_)
        lock.clear(std::memory_order_relaxed);

    for (auto& count : delayCounts_)
        count = 0;

    for (auto& time : latestReturnTimes_)
        time = std::chrono::steady_clock::now();
}

CentralCache& CentralCache::getInstance() {
    static CentralCache instance;
    return instance;
}

void* CentralCache::allocateBatch(size_t index) {

    if (index >= Size::FREE_LIST_SIZE) return nullptr;

    size_t size = (index + 1) * Size::ALIGNMENT;
    size_t numBlocks = CentralToThreadStrategy(index);

    SpinLockGuard lock(locks_[index]);

    void* result = freeList_[index];

    if (!result)
    {
        size_t numPages = PageToCentralStrategy(index);
        result = fetchFromPageCache(index);

        if (!result) return nullptr;

        char* head = static_cast<char*>(result);
        size_t totalBlocks = (numPages * Size::PAGE_SIZE) / size;
        size_t allocBlocks = std::min(numBlocks, totalBlocks);

        {
            std::unique_lock pmLock(pageMapMutex_);

            uintptr_t spanKey = reinterpret_cast<uintptr_t>(result);
            SpanTracker& tracker = spanStore_[spanKey];
            tracker.init(result, numPages, totalBlocks);

            size_t basePage = spanKey / Size::PAGE_SIZE;
            for (size_t p = 0; p < numPages; ++p) 
                pageMap_[basePage + p] = &tracker;
        }

        // Build the list returned to ThreadCache
        for (size_t i = 1; i < allocBlocks; ++i) {
            void* current = head + (i - 1) * size;
            void* next = head + i * size;
            *reinterpret_cast<void**>(current) = next;
        }
        *reinterpret_cast<void**>(head + (allocBlocks - 1) * size) = nullptr;

        // Remaining blocks stay on CentralCache free list
        if (totalBlocks > allocBlocks)
        {
            void* remainStart = head + allocBlocks * size;
            for (size_t i = allocBlocks + 1; i < totalBlocks; ++i) {
                void* current = head + (i - 1) * size;
                void* next = head + i * size;
                *reinterpret_cast<void**>(current) = next;
            }
            *reinterpret_cast<void**>(head + (totalBlocks - 1) * size) = nullptr;

            freeList_[index] = remainStart;
        }
    }
    else
    {
        // Take numBlocks nodes from existing list
        void* current = result;
        void* prev = nullptr;
        size_t count = 0;

        while (current && count < numBlocks) {
            prev = current;
            current = *reinterpret_cast<void**>(current);
            ++count;
        }
        if (prev) 
            *reinterpret_cast<void**>(prev) = nullptr;

        freeList_[index] = current;
    }

    return result;
}

void* CentralCache::fetchFromPageCache(size_t index) {
    size_t numPages = PageToCentralStrategy(index);
    return PageCache::getInstance().allocateSpan(numPages);
}

void CentralCache::deallocateBatch(void* ptr, size_t numReturn, size_t index) {
    if (ptr == nullptr || numReturn == 0 || index >= Size::FREE_LIST_SIZE)
        return;

    SpinLockGuard lock(locks_[index]);

    // Find the last block returned
    void* tail = ptr;
    size_t count = 1;
    while (*reinterpret_cast<void**>(tail) != nullptr && count < numReturn) {
        tail = *reinterpret_cast<void**>(tail);
        ++count;
    }

    // Splice central head after returned tail
    *reinterpret_cast<void**>(tail) = freeList_[index];
    freeList_[index] = ptr;

    size_t currentCount = delayCounts_[index] + count;
    auto   currentTime = std::chrono::steady_clock::now();
    if (shouldReturn(index, currentCount, currentTime)) 
        tryReclaimSpans(index);
}

bool CentralCache::shouldReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime) {
    if (currentCount >= MAX_DELAY_COUNT)
        return true;

    auto lastTime = latestReturnTimes_[index];
    return (currentTime - lastTime) >= MAX_DELAY_DURATION;
}

void CentralCache::tryReclaimSpans(size_t index)
{
    delayCounts_[index] = 0;
    latestReturnTimes_[index] = std::chrono::steady_clock::now();

    std::unordered_map<SpanTracker*, size_t> spanFreeCounts;
    {
        std::shared_lock pmLock(pageMapMutex_);

        void* currentBlock = freeList_[index];
        while (currentBlock)
        {
            SpanTracker* tracker = getSpanTracker(currentBlock);
            if (tracker)
                spanFreeCounts[tracker]++;

            currentBlock = *reinterpret_cast<void**>(currentBlock);
        }
    }

    for (const auto& [tracker, newFreeBlocks] : spanFreeCounts)
        returnSpan(tracker, newFreeBlocks, index);
}

void CentralCache::returnSpan(SpanTracker* tracker, size_t freeCount, size_t index) {
    if (freeCount == tracker->blockCount)
    {
        void* spanAddr = tracker->spanAddr;
        size_t numPages = tracker->numPages;

        // Remove blocks of this span from free list
        void* head = freeList_[index];
        void* newHead = nullptr;
        void* prev = nullptr;
        void* current = head;

        while (current)
        {
            void* next = *reinterpret_cast<void**>(current);
            if (current >= spanAddr &&
                current < static_cast<char*>(spanAddr) + numPages * Size::PAGE_SIZE)
            {
                if (prev)
                {
                    *reinterpret_cast<void**>(prev) = next;
                }
                else
                {
                    newHead = next;
                }
            }
            else
            {
                if (!prev) newHead = current;
                prev = current;
            }
            current = next;
        }

        freeList_[index];
        PageCache::getInstance().deallocateSpan(spanAddr, numPages);

        {
            std::unique_lock pmLock(pageMapMutex_);
            unregisterSpan(*tracker);
        }

    }
}

size_t CentralCache::CentralToThreadStrategy(size_t index) {
    const size_t sz = (index + 1) * Size::ALIGNMENT;
    if (sz <= 64)
        return 160;
    if (sz <= 128)
        return 128;
    if (sz <= 256)
        return 64;
    if (sz <= 512)
        return 32;
    if (sz <= 1024)
        return 24;
    return 1000;
}

size_t CentralCache::PageToCentralStrategy(size_t index) {
    const size_t sz = (index + 1) * Size::ALIGNMENT;
    const size_t batch = CentralToThreadStrategy(index);

    size_t k;
    if (sz <= 64)
        k = 12;
    else if (sz <= 128)
        k = 10;
    else if (sz <= 256)
        k = 8;
    else if (sz <= 512)
        k = 6;
    else
        k = 4;

    size_t targetBlocks = batch * k;
    size_t bytesNeeded = targetBlocks * sz;

    size_t pages = (bytesNeeded + Size::PAGE_SIZE - 1) / Size::PAGE_SIZE;
    const size_t MAX_PAGES = (sz <= 128) ? 16 : (sz <= 512 ? 8 : 4);

    if (pages < 1) pages = 1;
    if (pages > MAX_PAGES) pages = MAX_PAGES;

    return pages;
}

void CentralCache::unregisterSpan(SpanTracker& tracker) {
    void* addr = tracker.spanAddr;
    size_t numPages = tracker.numPages;

    size_t basePage = reinterpret_cast<uintptr_t>(addr) / Size::PAGE_SIZE;
    for (size_t p = 0; p < numPages; ++p) {
        pageMap_.erase(basePage + p);
    }

    uintptr_t spanKey = reinterpret_cast<uintptr_t>(addr);
    spanStore_.erase(spanKey);
}

// find which page the block belongs to, 
// then find the span tracker for that page using page map.
SpanTracker* CentralCache::getSpanTracker(void* blockAddr)
{
    size_t pageNum = reinterpret_cast<uintptr_t>(blockAddr) / Size::PAGE_SIZE;
    auto it = pageMap_.find(pageNum);
    if (it != pageMap_.end()) return it->second;
    return nullptr;
}