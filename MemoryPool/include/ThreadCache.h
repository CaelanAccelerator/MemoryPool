#pragma once

#include<array>

class ThreadCache
{
public:
	ThreadCache();
	~ThreadCache();
	static ThreadCache& getinstance();
	void* allocate();
	void deallocate();

private:

	std::array<void*, 128> freeList_;
	std::array<size_t, 128> freeListSize_;

	void* fetchFromCentralCache(size_t);
	void* returnToCentralCache(void* ptr, size_t);
	bool shouldReturn();
};

ThreadCache::ThreadCache()
{
}

ThreadCache::~ThreadCache()
{
}