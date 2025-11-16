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
		//remove spanReturned from the freeSpans_ list
		Span* spanReturned = it->second;
		if (spanReturned->next)
		{
			freeSpans_[it->first] = spanReturned->next;
		}
		else
		{
			freeSpans_.erase(it);
		}

		if (spanReturned->numPages > numPages)
		{
			void* newSpanAddr = static_cast<char*>(spanReturned->addr) + numPages * Size::PAGE_SIZE;
			Span* newSpan = new Span;
			newSpan->numPages = spanReturned->numPages - numPages;
			newSpan->addr = newSpanAddr;

			//head insertion to add the newSpan to freeSpans_ list
			newSpan->next = freeSpans_[newSpan->numPages];
			freeSpans_[newSpan->numPages] = newSpan;

			spanMap_[newSpanAddr] = newSpan;
		}

		spanReturned->numPages = numPages;
		spanMap_[spanReturned->addr] = spanReturned;
		return spanReturned->addr;
	}
	void* newSpanAddr = systemAlloc(numPages);
	if (!newSpanAddr) return nullptr;
	Span* newSpan = new Span;
	newSpan->addr = newSpanAddr;
	newSpan->numPages = numPages;
	newSpan->next = nullptr;

	spanMap_[newSpan->addr] = newSpan;
	return newSpan->addr;

}

void PageCache::deallocateSpan(void* spanAddr, size_t numPages) {
	std::lock_guard<std::mutex> lock(mutexLock);

	auto it = spanMap_.find(spanAddr);
	if (it == spanMap_.end()) return;
	Span* span = it->second;

	//try to merge spans
	void* nextSpanAddr = static_cast<char*>(spanAddr) + span->numPages * Size::PAGE_SIZE;
	it = spanMap_.find(nextSpanAddr);
	if (it != spanMap_.end())
	{
		Span* nextSpan = it->second;

		Span* cur = freeSpans_[nextSpan->numPages];
		Span* prev = nullptr;
		bool found = false;
		while (cur)
		{
			if (cur == nextSpan) {
				found = true;
				if (prev) {
					prev->next = cur->next;
					break;
				}
				else {
					freeSpans_[nextSpan->numPages] = freeSpans_[nextSpan->numPages]->next;
					break; 
				}
			}
			prev = cur;
			cur = cur->next;
		}

		if (found)
		{
			span->numPages += nextSpan->numPages;
			spanMap_.erase(nextSpanAddr);
			delete nextSpan;
		}
	}

	//head insertion to add return the span to the freeSpans_
	span->next = freeSpans_[span->numPages];
	freeSpans_[span->numPages] = span;
}

void* PageCache::systemAlloc(size_t numPages) {
	size_t size = numPages * Size::PAGE_SIZE;

#if defined(_WIN32)
	// Windows: 
	void* ptr = ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	return ptr; 
#else
	// Linux/macOS: POSIX mmap
	void* ptr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) return nullptr;
	return ptr;
#endif
}