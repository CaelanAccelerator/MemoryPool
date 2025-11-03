#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

ThreadCache& ThreadCache::getinstance() {
	static thread_local ThreadCache instance;
	return instance;
}

void* ThreadCache::allocate(size_t size) {
	// Boundary Cases:
	if (size == 0) return nullptr;
	if (size > MAX_SIZE) return malloc(size);
	// FreeList is not empty:
	size_t index = size / 8;
	if (freeList_[index]) {
		void* ptr = freeList_[index];
		freeList_[index] = *reinterpret_cast<void**>(freeList_[index]);
		--freeListSize_[index];
		return ptr;
	}
	// FreeList is empty, fetch from Central Cache:
	return fetchFromCentralCache(index);
}

void ThreadCache::deallocate(void* ptr, size_t size) {
	// Boundary Cases:
	if (size == 0 || ptr == nullptr) return;
	if (size > MAX_SIZE)
	{
		free(ptr);
		return;
	}

	size_t index = size / 8;
	*reinterpret_cast<void**>(ptr) = freeList_[index];
	freeList_[index] = ptr;
	++freeListSize_[index];
	if (shouldReturn(index))
		returnToCentralCache(freeList_[index], size);
}

void* ThreadCache::fetchFromCentralCache(size_t index) {
	void* ptr = CentralCache::getInstance().fetchToThreadCache(index);
	if (!ptr) return nullptr;
	
	// return the result to user, put the rest into freeList_
	void* result = ptr;
	ptr = *reinterpret_cast<void**>(ptr);
	freeList_[index] = ptr;

	//update the freeListSize
	size_t batchNum{ 0 };
	while (ptr)
	{
		batchNum++;
		ptr = *reinterpret_cast<void**>(ptr);
	}
	freeListSize_[index] += batchNum;
	return result;
}

void ThreadCache::returnToCentralCache(void* ptr, size_t size) {
	size_t index = size / 8;
	
	size_t numBatch = freeListSize_[index];
	if (numBatch == 1) return;
	
	size_t numKeep = std::max(numBatch / 4, size_t(1));
	size_t numReturn = numBatch - numKeep;
	for (size_t i = 1; i < numKeep && ptr; i++)
	{
		ptr = *reinterpret_cast<void**>(ptr);
	}

	freeListSize_[index] = numKeep;
	void* nodeReturn = *reinterpret_cast<void**>(ptr);
	*reinterpret_cast<void**>(ptr) = nullptr;

	CentralCache::getInstance().receiveFromThreadCache(nodeReturn, numReturn, index);
}

bool ThreadCache::shouldReturn(size_t index) {
	size_t threshold = 256;
	return freeListSize_[index] > threshold;
}