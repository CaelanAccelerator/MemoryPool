#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"
#include <cstddef>
using std::size_t;

ThreadCache &ThreadCache::getInstance()
{
	static thread_local ThreadCache instance;
	return instance;
}

void *ThreadCache::allocate(size_t size)
{
	// Boundary Cases:
	if (size == 0)
		return nullptr;
	if (size > Size::MAX_ALLOC_SIZE)
		return malloc(size);

	// Free lists are implemented as singly linked lists using LIFO
	// (head insertion + head removal).
	size_t index = Size::sizeToIndex(size);
	if (freeListEntries_[index].head)
	{
		void *ptr = freeListEntries_[index].head;
		freeListEntries_[index].head = *reinterpret_cast<void **>(freeListEntries_[index].head);
		if (freeListEntries_[index].head == nullptr)
			freeListEntries_[index].tail = nullptr;
		--freeListEntries_[index].size;
		return ptr;
	}
	// if empty, fetch from Central Cache:
	return refillFromCentral(index);
}

void ThreadCache::deallocate(void *ptr, size_t size)
{
	// Boundary Cases:
	if (size == 0 || ptr == nullptr)
		return;
	if (size > Size::MAX_ALLOC_SIZE)
	{
		free(ptr);
		return;
	}

	size_t index = Size::sizeToIndex(size);
	*reinterpret_cast<void **>(ptr) = freeListEntries_[index].head;
	if (freeListEntries_[index].tail == nullptr)
		freeListEntries_[index].tail = ptr;
	freeListEntries_[index].head = ptr;
	++freeListEntries_[index].size;
	if (shouldReturn(index))
		drainToCentral(freeListEntries_[index].head, freeListEntries_[index].tail, size);
}

void *ThreadCache::refillFromCentral(size_t index)
{
	void *ptr = CentralCache::getInstance().allocateBatch(index);
	if (!ptr)
		return nullptr;

	// Save the first node to return to user
	// Move ptr to the next node for freeList
	// Update the freeListSize
	void *result = ptr;
	ptr = *reinterpret_cast<void **>(ptr);
	if (ptr == nullptr)
	{
		freeListEntries_[index].tail = nullptr;
		return result;
	}

	freeListEntries_[index].head = ptr;

	size_t batchNum{0};
	while (*reinterpret_cast<void **>(ptr) != nullptr)
	{
		batchNum++;
		ptr = *reinterpret_cast<void **>(ptr);
	}

	batchNum++;
	freeListEntries_[index].tail = ptr;
	freeListEntries_[index].size += batchNum;
	return result;
}

void ThreadCache::drainToCentral(void *ptr, void *tail, size_t size)
{
	if (!ptr)
		return;

	size_t index = Size::sizeToIndex(size);

	size_t numBatch = freeListEntries_[index].size;
	if (numBatch == 1)
		return;

	// keep 50% of the blocks in the thread cache, return 1/2
	size_t numKeep = std::max(numBatch / 2, size_t(1));
	size_t numReturn = numBatch - numKeep;
	for (size_t i = 1; i < numKeep && ptr; i++)
		ptr = *reinterpret_cast<void **>(ptr);

	freeListEntries_[index].tail = ptr;
	freeListEntries_[index].size = numKeep;
	void *nodeReturn = *reinterpret_cast<void **>(ptr);
	*reinterpret_cast<void **>(ptr) = nullptr;
	CentralCache::getInstance().deallocateBatch(nodeReturn, tail, numReturn, index);
};

bool ThreadCache::shouldReturn(size_t index)
{
	size_t blockSize = Size::indexToBlockSize(index);
	size_t threshold = (512 * 1024) / blockSize;
	threshold = std::max(threshold, size_t(16));
	return freeListEntries_[index].size > threshold;
}