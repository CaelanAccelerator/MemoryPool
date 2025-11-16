#include "../include/CentralCache.h"
#include <thread>
#include <PageCache.h>
#include <unordered_map>
#include "Size.h"

const std::chrono::milliseconds CentralCache::MAX_DELAY_DURATION{ 1000 };

CentralCache::CentralCache()
{
	for (auto& ptr : freeList_)
	{
		ptr.store(nullptr, std::memory_order_relaxed); 
	}
	
	for (auto& lock : locks_)
	{
		lock.clear(std::memory_order_relaxed);
	}
	
	for (auto& count : delayCounts_)
	{
		count.store(0, std::memory_order_relaxed);
	}
	
	for (auto& time : latestReturnTimes_)
	{
		time = std::chrono::steady_clock::now();
	}
	spanCount.store(0, std::memory_order_relaxed);
}

CentralCache& CentralCache::getInstance() {
	static CentralCache instance;
	return instance;
}

void* CentralCache::fetchToThreadCache(size_t index) {

	size_t size = (index + 1) * 8;
	size_t numBatch = CentralToThreadStrategy(index);

	while (locks_[index].test_and_set(std::memory_order_acquire))
	{
		std::this_thread::yield();
	}

	void* result{ nullptr };
	try
	{
		result = freeList_[index].load(std::memory_order_relaxed);
		if (!result)
		{
			size_t numPages = PageToCentralStrategy(index);
			result = fetchFromPageCache(index);

			if (!result)
			{
				locks_[index].clear(std::memory_order_release);
				return nullptr;
			}

			char* head = static_cast<char*>(result);
			size_t totalBlocks = (numPages * Size::PAGE_SIZE) / size;
			size_t allocBlocks = std::min(numBatch, totalBlocks);
 
			// Build the list returned to the ThreadCache
			for (size_t i = 1; i < allocBlocks; ++i)
			{
				void* current = head + (i - 1) * size;
				void* next = head + i * size;
				*reinterpret_cast<void**>(current) = next;
			}
			*reinterpret_cast<void**>(head + (allocBlocks - 1) * size) = nullptr;

			// Build the remaining list kept in CentralCache
			if (totalBlocks > allocBlocks)
			{
				void* remainStart = head + allocBlocks * size;
				for (size_t i = allocBlocks + 1; i < totalBlocks; ++i)
				{
					void* current = head + (i - 1) * size;
					void* next = head + i * size;
					*reinterpret_cast<void**>(current) = next;
				}
				*reinterpret_cast<void**>(head + (totalBlocks - 1) * size) = nullptr;

				freeList_[index].store(remainStart, std::memory_order_relaxed);
			}
			size_t trackerIndex = spanCount.fetch_add(1, std::memory_order_acq_rel);
			if (trackerIndex >= spanTrackers_.size()) {
				// Slot array full: rollback and skip registration to avoid out-of-bounds
				spanCount.fetch_sub(1, std::memory_order_relaxed);
			}
			else {
				spanTrackers_[trackerIndex].spanAddr.store(result, std::memory_order_release);
				spanTrackers_[trackerIndex].blockCount.store(totalBlocks, std::memory_order_release);
				spanTrackers_[trackerIndex].freeCount.store(totalBlocks - allocBlocks, std::memory_order_release);
				spanTrackers_[trackerIndex].numPages.store(numPages, std::memory_order_release);
			}

		}
		else
		{
			void* current = result;
			void* prev = nullptr;
			size_t count = 0;

			while (current && count < numBatch)
			{
				prev = current;
				SpanTracker* tracker = getSpanTracker(prev);
				if (tracker)
					tracker->freeCount.fetch_sub(1, std::memory_order_relaxed);
				current = *reinterpret_cast<void**>(current);
				count++;
			}

			if (prev) 
			{
				*reinterpret_cast<void**>(prev) = nullptr;
			}

			freeList_[index].store(current, std::memory_order_release);
		}
	}
	catch (...)
	{
		locks_[index].clear(std::memory_order_release);
		throw;
	}
	locks_[index].clear(std::memory_order_release);
	return result;
}

void* CentralCache::fetchFromPageCache(size_t index) {
	size_t numPages = PageToCentralStrategy(index);
	return PageCache::getInstance().allocateSpan(numPages);
}

void CentralCache::receiveFromThreadCache(void* ptr, size_t numReturn, size_t index) {
	if (ptr == nullptr || numReturn == 0 || index >= Size::FREE_LIST_SIZE)
		return;
	while (locks_[index].test_and_set(std::memory_order_acquire))
	{
		std::this_thread::yield();
	}
	try
	{
		void* tail = ptr; 
		size_t count = 1;

		// Find last block in returned chain (bounded by numReturn)
		while (*reinterpret_cast<void**>(tail) != nullptr && count < numReturn)
		{
			SpanTracker* tracker = getSpanTracker(tail);
			if (tracker)
				tracker->freeCount.fetch_add(1, std::memory_order_relaxed);
			tail = *reinterpret_cast<void**>(tail);
			++count;
		}
		if (tail) {
			SpanTracker* tr = getSpanTracker(tail);
			if (tr) tr->freeCount.fetch_add(1, std::memory_order_relaxed);
		}
		// Link existing CentralCache head after tail and update head
		*reinterpret_cast<void**>(tail) = freeList_[index].load(std::memory_order_relaxed);
		freeList_[index].store(ptr, std::memory_order_relaxed);

		size_t currentCount = delayCounts_[index].fetch_add(count, std::memory_order_relaxed) + count;
		auto currentTime = std::chrono::steady_clock::now();
		if (shouldReturn(index, currentCount, currentTime))
		{
			performDelayReturn(index);
		}
	}
	catch (...)
	{
		locks_[index].clear(std::memory_order_release);
		throw;
	}
	locks_[index].clear(std::memory_order_release);
}

bool CentralCache::shouldReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime) {
	if (currentCount >= MAX_DELAY_COUNT)
	{
		return true;
	}

	auto lastTime = latestReturnTimes_[index];
	return (currentTime - lastTime) >= MAX_DELAY_DURATION;
}

void CentralCache::performDelayReturn(size_t index) {
	size_t i = 0;
	while (i < spanCount.load(std::memory_order_relaxed)) {
		SpanTracker& t = spanTrackers_[i];
		if (t.freeCount.load(std::memory_order_relaxed) ==
			t.blockCount.load(std::memory_order_relaxed)) {
			returnSpan(&t, index);
			recycleSpanSlot(t);   
			// After recycle: element moved into position i, re-check same index
		}
		else {
			++i;
		}
	}
	delayCounts_[index].store(0, std::memory_order_relaxed);
	latestReturnTimes_[index] = std::chrono::steady_clock::now();
}

SpanTracker* CentralCache::getSpanTracker(void* blockAddr) {
	if (!blockAddr) return nullptr;
	size_t count = spanCount.load(std::memory_order_relaxed);
	for (size_t i = 0; i < count; i++)
	{
		void* spanAddr = spanTrackers_[i].spanAddr.load(std::memory_order_relaxed);
		size_t numPages = spanTrackers_[i].numPages.load(std::memory_order_relaxed);

		if (blockAddr >= spanAddr && blockAddr < static_cast<char*>(spanAddr) + numPages * Size::PAGE_SIZE)
		{
			return &spanTrackers_[i];
		}
	}
	return nullptr;
}

void CentralCache::returnSpan(SpanTracker* tracker, size_t index) {
	size_t numPages = tracker->numPages.load(std::memory_order_relaxed);
	void* spanAddr = tracker->spanAddr.load(std::memory_order_relaxed);
	void* currentBlock = freeList_[index].load(std::memory_order_relaxed);
	void* newHead = nullptr;
	void* prev = nullptr;
	while (currentBlock)
	{
		if (currentBlock < spanAddr || currentBlock >= reinterpret_cast<char*>(spanAddr) + Size::PAGE_SIZE * numPages)
		{
			if (!prev)
			{
				newHead = currentBlock;
				prev = currentBlock;
			}
			else
			{
				*reinterpret_cast<void**>(prev) = currentBlock;
				prev = currentBlock;
			}
		}
		currentBlock = *reinterpret_cast<void**>(currentBlock);
	}

	// Detach the filtered list
	if (prev) {                                   
		*reinterpret_cast<void**>(prev) = nullptr;
	}

	freeList_[index].store(newHead, std::memory_order_relaxed);
	PageCache::getInstance().deallocateSpan(spanAddr, numPages);
}

// Goal: fewer lock acquisitions, avoid over-accumulation, avoid exhausting spanTrackers_
size_t CentralCache::CentralToThreadStrategy(size_t index) {
	const size_t sz = (index + 1) * 8; // object size
	// Smaller objects: larger batch to amortize lock; larger objects: smaller batch
	if (sz <= 64)   return 160; 
	if (sz <= 128)  return 128;
	if (sz <= 256)  return 64;
	if (sz <= 512)  return 32;
	if (sz <= 1024) return 24;
	return 1000;                    
}

size_t CentralCache::PageToCentralStrategy(size_t index) {
	const size_t sz = (index + 1) * 8;
	const size_t batch = CentralToThreadStrategy(index);

	// Target: totalBlocks ≈ k * batch. Larger k for small objects, smaller for big ones.
	size_t k;
	if (sz <= 64)       k = 12;
	else if (sz <= 128) k = 10;
	else if (sz <= 256) k = 8;
	else if (sz <= 512) k = 6;
	else                k = 4;

	size_t targetBlocks = batch * k;

	// Derive pages needed: ceil(targetBlocks * sz / PAGE_SIZE)
	size_t bytesNeeded = targetBlocks * sz;
	size_t pages = (bytesNeeded + Size::PAGE_SIZE - 1) / Size::PAGE_SIZE;

	// Safety cap: prevent pulling too many pages at once
	const size_t MAX_PAGES =
		(sz <= 128) ? 16 :
		(sz <= 512) ? 8 :
		4;
	if (pages < 1)       pages = 1;
	if (pages > MAX_PAGES) pages = MAX_PAGES;

	return pages;
}

void CentralCache::recycleSpanSlot(SpanTracker& tracker) {
	SpanTracker& back = spanTrackers_.back();
	if (&tracker != &back)
	{
		tracker.spanAddr.store(back.spanAddr.load(std::memory_order_relaxed), std::memory_order_relaxed);
		tracker.blockCount.store(back.blockCount.load(std::memory_order_relaxed), std::memory_order_relaxed);
		tracker.freeCount.store(back.freeCount.load(std::memory_order_relaxed), std::memory_order_relaxed);
		tracker.numPages.store(back.numPages.load(std::memory_order_relaxed), std::memory_order_relaxed);
	}
	spanCount.fetch_sub(1, std::memory_order_relaxed);
}