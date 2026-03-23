#pragma once

#include<array>
#include"Size.h"
#include <cstddef>
using std::size_t;

class ThreadCache
{
public:
	
	//Use the Singleton pattern
	static ThreadCache& getInstance();
	void* allocate(size_t);
	void deallocate(void* ptr, size_t);

private:
	ThreadCache()
	{
		freeList_.fill(nullptr);
		freeListSize_.fill(0);
	}
	std::array<void*, Size::FREE_LIST_SIZE> freeList_;
	std::array<size_t, Size::FREE_LIST_SIZE> freeListSize_;

	void* refillFromCentral(size_t);
	void drainToCentral(void* ptr, size_t);
	bool shouldReturn(size_t index);
};

