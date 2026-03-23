#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"
#include <cstddef>
using std::size_t;

ThreadCache& ThreadCache::getInstance() {
	static thread_local ThreadCache instance;
	return instance;
}

void* ThreadCache::allocate(size_t size) {
	// Boundary Cases:
	if (size == 0) return nullptr;
	if (size > Size::MAX_ALLOC_SIZE) return malloc(size);

	// Free lists are implemented as singly linked lists using LIFO
	// (head insertion + head removal). 
	size_t index = (size - 1) / 8;
	if (freeList_[index]) {
		void* ptr = freeList_[index];
		freeList_[index] = *reinterpret_cast<void**>(freeList_[index]);
		--freeListSize_[index];
		return ptr;
	}
	// if empty, fetch from Central Cache:
	return refillFromCentral(index);
}

void ThreadCache::deallocate(void* ptr, size_t size) {
	// Boundary Cases:
	if (size == 0 || ptr == nullptr) return;
	if (size > Size::MAX_ALLOC_SIZE)
	{
		free(ptr);
		return;
	}

	size_t index = (size - 1) / 8;
	*reinterpret_cast<void**>(ptr) = freeList_[index];
	freeList_[index] = ptr;
	++freeListSize_[index];
	if (shouldReturn(index))
		drainToCentral(freeList_[index], size);
}

void* ThreadCache::refillFromCentral(size_t index) {
	void* ptr = CentralCache::getInstance().allocateBatch(index);
	if (!ptr) return nullptr;
	
	// Save the first node to return to user
	// Move ptr to the next node for freeList
	// Update the freeListSize
	void* result = ptr;
	ptr = *reinterpret_cast<void**>(ptr);
	freeList_[index] = ptr;

	size_t batchNum{ 0 };
	while (ptr)
	{
		batchNum++;
		ptr = *reinterpret_cast<void**>(ptr);
	}
	freeListSize_[index] += batchNum;
	return result;
}

void ThreadCache::drainToCentral(void* ptr, size_t size) {
	if (!ptr) return;

	size_t index = (size - 1) / 8;

	size_t numBatch = freeListSize_[index];
	if (numBatch == 1) return;

	// keep 25% of the blocks in the thread cache, return 3/4 per term
	size_t numKeep = std::max(numBatch / 4, size_t(1));
	size_t numReturn = numBatch - numKeep;
	for (size_t i = 1; i < numKeep && ptr; i++)
		ptr = *reinterpret_cast<void**>(ptr);

	freeListSize_[index] = numKeep;
	void* nodeReturn = *reinterpret_cast<void**>(ptr);
	*reinterpret_cast<void**>(ptr) = nullptr;
	CentralCache::getInstance().deallocateBatch(nodeReturn, numReturn, index);

};

bool ThreadCache::shouldReturn(size_t index) {
	size_t blockSize = (index + 1) * Size::ALIGNMENT;
	size_t threshold = (64 * 1024) / blockSize;
	threshold = std::max(threshold, size_t(16));
	return freeListSize_[index] > threshold;
}