#pragma once

#include<atomic>
#include<array>
#include<chrono>
#include <vector>
#include"Size.h"
struct SpanTracker
{
	std::atomic<void*> spanAddr{ nullptr };
	std::atomic<size_t> numPages{ 0 };
	std::atomic<size_t> blockCount{ 0 };
};

class CentralCache
{
public:

	//Use singlton pattern
	static CentralCache& getInstance();
	void* fetchToThreadCache(size_t);
	void receiveFromThreadCache(void* ptr, size_t numReturn, size_t index);

private:
	CentralCache();
	void* fetchFromPageCache(size_t);
	void returnSpan(SpanTracker*, size_t newFreeBlocks, size_t index);

	//Central Free List and locks
	std::array<std::atomic<void*>, Size::FREE_LIST_SIZE> freeList_;
	std::array<std::atomic_flag, Size::FREE_LIST_SIZE> locks_;

	//use array to store span info to reduce the cost by map
	std::array<SpanTracker, Size::FREE_LIST_SIZE> spanTrackers_;
	std::atomic<size_t> spanCount{ 0 };

	//Counters for return decisions
	static const size_t MAX_DELAY_COUNT{ 48 };
	std::array<std::atomic<size_t>, Size::FREE_LIST_SIZE> delayCounts_;
	static const std::chrono::milliseconds MAX_DELAY_DURATION;
	std::array<std::chrono::steady_clock::time_point, Size::FREE_LIST_SIZE> latestReturnTimes_;

	bool shouldReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTIme);
	void performDelayReturn(size_t index);
	SpanTracker* getSpanTracker(void* blockAddr);
	void recycleSpanSlot(SpanTracker& tracker);
	size_t PageToCentralStrategy(size_t index);
	size_t CentralToThreadStrategy(size_t index);
};


