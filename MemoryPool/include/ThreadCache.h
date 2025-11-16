#pragma once

#include<array>
#include"Size.h"

class ThreadCache
{
public:
	
	//Use the Singlton pattern
	static ThreadCache& getInstance();
	void* allocate(size_t);
	void deallocate(void* ptr, size_t);

private:

	static const size_t MAX_SIZE{ 512 };
	ThreadCache()
	{
		freeList_.fill(nullptr);
		freeListSize_.fill(0);
	}
	std::array<void*, Size::FREE_LIST_SIZE> freeList_;
	std::array<size_t, Size::FREE_LIST_SIZE> freeListSize_;

	void* fetchFromCentralCache(size_t);
	void returnToCentralCache(void* ptr, size_t);
	bool shouldReturn(size_t index);
};

