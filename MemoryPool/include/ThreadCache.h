#pragma once

#include<array>

class ThreadCache
{
public:
	
	//Use the Singlton pattern
	static ThreadCache& getinstance();
	void* allocate(size_t);
	void deallocate(void* ptr, size_t);

private:
	static const size_t MAX_SIZE{ 512 };
	ThreadCache();
	std::array<void*, 128> freeList_;
	std::array<size_t, 128> freeListSize_;

	void* fetchFromCentralCache(size_t);
	void returnToCentralCache(void* ptr, size_t);
	bool shouldReturn(size_t index);
};

ThreadCache::ThreadCache()
{
	freeList_.fill(nullptr);
	freeListSize_.fill(0);
}
