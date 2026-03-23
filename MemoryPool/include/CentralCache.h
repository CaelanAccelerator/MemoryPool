#pragma once
#include<atomic>
#include <array>
#include <chrono>
#include <shared_mutex>
#include <unordered_map>
#include "Size.h"
#include <cstddef>
using std::size_t;

struct SpanTracker
{
	void* spanAddr{ nullptr };
	size_t numPages{ 0 };
	size_t blockCount{ 0 };

	void init(void* addr, size_t pages, size_t blocks) {
		spanAddr = addr;
		numPages = pages;
		blockCount = blocks;
	}
};

class CentralCache
{
public:
	static CentralCache& getInstance();
	void* allocateBatch(size_t index);
	void deallocateBatch(void* ptr, size_t numReturn, size_t index);

private:
	CentralCache();
	void* fetchFromPageCache(size_t);
	void returnSpan(SpanTracker*, size_t freeCount, size_t index);

	// per-bucket free lists and locks
	std::array<void*, Size::FREE_LIST_SIZE> freeList_;
	std::array<std::atomic_flag, Size::FREE_LIST_SIZE> locks_;

	// page map: shared across ALL buckets, needs its own lock.
	// lock ordering: locks_[i] -> pageMapMutex_ -> PageCache::mutex_
	std::unordered_map<size_t, SpanTracker*> pageMap_;
	std::unordered_map<uintptr_t, SpanTracker> spanStore_;
	mutable std::shared_mutex pageMapMutex_;

	// delayed return heuristic
	static const size_t MAX_DELAY_COUNT{ 48 };
	static const std::chrono::milliseconds MAX_DELAY_DURATION;
	std::array<size_t, Size::FREE_LIST_SIZE> delayCounts_;
	std::array<std::chrono::steady_clock::time_point, Size::FREE_LIST_SIZE> latestReturnTimes_;

	bool shouldReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime);
	void tryReclaimSpans(size_t index);
	SpanTracker* getSpanTracker(void* blockAddr);  // caller holds shared lock
	void unregisterSpan(SpanTracker& tracker);      // caller holds unique lock
	size_t PageToCentralStrategy(size_t index);
	size_t CentralToThreadStrategy(size_t index);
};

