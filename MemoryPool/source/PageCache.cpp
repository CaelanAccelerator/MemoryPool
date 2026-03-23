#include "../include/PageCache.h"
#include "Size.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif


void* PageCache::allocateSpan(size_t numPages) {
	std::lock_guard<std::mutex> lock(mutexLock);

	auto it = freeSpans_.lower_bound(numPages);
	if (it != freeSpans_.end())
	{
		//remove spanToReturn from the freeSpans_ list
		Span* spanToReturn = it->second;
		if (spanToReturn->next)
			freeSpans_[it->first] = spanToReturn->next;
		else
			freeSpans_.erase(it);
		void* oldEnd = static_cast<char*>(spanToReturn->addr)
			+ spanToReturn->numPages * Size::PAGE_SIZE;
		endMap_.erase(oldEnd);
	
		if (spanToReturn->numPages > numPages)
		{
			void* newSpanAddr = static_cast<char*>(spanToReturn->addr) + numPages * Size::PAGE_SIZE;
			
			Span* newSpan = allocSpanMeta();
			newSpan->numPages = spanToReturn->numPages - numPages;
			newSpan->addr = newSpanAddr;

			//head insertion to add the newSpan to freeSpans_ list
			newSpan->next = freeSpans_[newSpan->numPages];
			freeSpans_[newSpan->numPages] = newSpan;

			spanMap_[newSpanAddr] = newSpan;

			void* newEnd = static_cast<char*>(newSpanAddr)
				+ newSpan->numPages * Size::PAGE_SIZE;
			endMap_[newEnd] = newSpan;
		}

		spanToReturn->numPages = numPages;
		spanMap_[spanToReturn->addr] = spanToReturn;
		return spanToReturn->addr;
	}

	void* newSpanAddr = systemAlloc(numPages);
	if (!newSpanAddr) return nullptr;

	Span* newSpan = allocSpanMeta();
	newSpan->addr = newSpanAddr;
	newSpan->numPages = numPages;
	newSpan->next = nullptr;

	spanMap_[newSpan->addr] = newSpan;
	return newSpan->addr;
}


// Helper: remove a span from its freeSpans_ bucket.
// Returns true if found and removed, false otherwise.
// Used by both forward and backward merge logic.
bool PageCache::removeFromFreeList(Span* target) {
	auto bucketIt = freeSpans_.find(target->numPages);
	if (bucketIt == freeSpans_.end()) return false;

	Span* cur = bucketIt->second;
	Span* prev = nullptr;
	while (cur) {
		if (cur == target) {
			if (prev) prev->next = cur->next;
			else {
				if (cur->next) bucketIt->second = cur->next;
				else freeSpans_.erase(bucketIt);
			}
			return true;
		}
		prev = cur;
		cur = cur->next;
	}
	return false;
}

void PageCache::deallocateSpan(void* spanAddr, size_t numPages) {
	std::lock_guard<std::mutex> lock(mutexLock);

	auto it = spanMap_.find(spanAddr);
	if (it == spanMap_.end()) return;
	Span* span = it->second;

	// forward merge 
	void* nextSpanAddr = static_cast<char*>(spanAddr) + span->numPages * Size::PAGE_SIZE;
	auto nextIt = spanMap_.find(nextSpanAddr);
	if (nextIt != spanMap_.end())
	{
		Span* nextSpan = nextIt->second;

		bool found = removeFromFreeList(nextSpan);
		if (found)
		{
			void* nextEnd = static_cast<char*>(nextSpanAddr) + nextSpan->numPages * Size::PAGE_SIZE;
			endMap_.erase(nextEnd);

			span->numPages += nextSpan->numPages;
			spanMap_.erase(nextSpanAddr);
			freeSpanMeta(nextSpan);
		}
	}

	// backward merge
	auto prevIt = endMap_.find(spanAddr);
	if (prevIt != endMap_.end())
	{
		Span* prevSpan = prevIt->second;

		bool found = removeFromFreeList(prevSpan);

		if (found)
		{
			endMap_.erase(prevIt);
			prevSpan->numPages += span->numPages;
			spanMap_.erase(spanAddr);
			freeSpanMeta(span);

			span = prevSpan;
			spanAddr = prevSpan->addr;
		}
	}

	// Insert merged span into freeSpans_
	span->next = freeSpans_[span->numPages];
	freeSpans_[span->numPages] = span;

	void* mergedEnd = static_cast<char*>(spanAddr) + span->numPages * Size::PAGE_SIZE;
	endMap_[mergedEnd] = span;
}



void* PageCache::systemAlloc(size_t numPages) {
	size_t size = numPages * Size::PAGE_SIZE;

#if defined(_WIN32)
	// Windows: 
	void* ptr = ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	return ptr;
#else
	// Linux/macOS: POSIX mmap
	void* ptr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) return nullptr;
	return ptr;
#endif
}


