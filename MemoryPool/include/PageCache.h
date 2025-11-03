#pragma once
#include<atomic>
#include<array>

class PageCache
{
#define FREE_LIST_SIZE 1024
	struct SpanTracker
	{
		std::atomic<size_t> spanAddr{ nullptr };
		std::atomic<size_t> numPages{ 0 };
		std::atomic<size_t> blockCount{ 0 };
		std::atomic<size_t>freeCount{ 0 };
	};
public:
	PageCache();
	~PageCache();
	void* fetch();
	void recive();
private:
	//freelists
	std::array<std::atomic<void*>, FREE_LIST_SIZE> freeList_;

	//spinLock
	std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
	//trackers
	std::array<SpanTracker, 1024> spanTrackers_;
	std::atomic<size_t> spanCounter{ 0 };
};

PageCache::PageCache()
{
}

PageCache::~PageCache()
{
}