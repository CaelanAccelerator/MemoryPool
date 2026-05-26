#pragma once

#include <array>
#include "Size.h"
#include <cstddef>
using std::size_t;

struct FreeListEntry
{
	void *head;
	void *tail;
	size_t size;

	FreeListEntry()
	{
		head = nullptr;
		tail = nullptr;
		size = 0;
	}
};

class ThreadCache
{
public:
	// Use the Singleton pattern
	static ThreadCache &getInstance();
	void *allocate(size_t);
	void deallocate(void *ptr, size_t);

private:
	ThreadCache()
	{
		freeListEntries_.fill(FreeListEntry());
	}
	std::array<FreeListEntry, Size::FREE_LIST_SIZE> freeListEntries_;

	void *refillFromCentral(size_t);
	void drainToCentral(void *head, void *tail, size_t);
	bool shouldReturn(size_t index);
};
