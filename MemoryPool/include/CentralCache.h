#pragma once

#include<atomic>
#include<array>
#include<chrono>
#define FREE_LIST_SIZE 1024
struct SpanTracker
{
	std::atomic<void*> spanAddr{ nullptr };
	std::atomic<size_t> numPages{ 0 };
	std::atomic<size_t> blockCount{ 0 };
	std::atomic<size_t>freeCount{ 0 };
};

class CentralCache
{
public:

	//Use singlton pattern
	static CentralCache& getInstance();
	void* fetchToThreadCache(size_t);
	void reciveFromThreadCache(void* ptr, size_t);

private:
	CentralCache();
	void* fetchFromPageCache(size_t);
	void* returnToPageCache(void* ptr, size_t);

	//Central Free List and locks
	std::array<void*, FREE_LIST_SIZE> freeList_;
	std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;

	//use array to store span info to reduce the cost by map
	std::array<SpanTracker, 1024> spanTrackers_;
	std::atomic<size_t> spanCount{ 0 };

	//Counters for return decisions
	static const size_t MAX_DELAY_COUNT{ 48 };
	std::array<std::atomic<size_t>, FREE_LIST_SIZE> delayCounts_;
	static const std::chrono::milliseconds MAX_DELAY_DURATION;
	std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> latestReturnTimes_;

	bool shouldReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTIme);
	void peformDelayReturn();
	/*std::array<short,>*/
};

CentralCache::CentralCache()
{
}

CentralCache::~CentralCache()
{
}