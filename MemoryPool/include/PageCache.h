#pragma once
#include<atomic>
#include<array>
#include<map>
#include<mutex>

class PageCache
{
public:
	static PageCache& getInstance() {
		static PageCache instance;
		return instance;
	}
	void* allocateSpan(size_t numPages);
	void deallocateSpan(void* spanAddr, size_t numPages);
private:
	PageCache() = default;
	struct Span
	{
		void* addr;
		size_t numPages;
		Span* next;
	};
	void* systemAlloc(size_t numPages);
	std::map<size_t, Span*> freeSpans_;
	std::map<void*, Span*> spanMap_;
	std::map<void*, Span*> endMap_;
	bool removeFromFreeList(Span* target);
	std::mutex mutexLock;

	static constexpr size_t MAX_SPANS = 4096;
	Span spanPool_[MAX_SPANS];
	Span* spanFreeList_ = nullptr;
	size_t spanPoolUsed_ = 0;

	Span* allocSpanMeta() {
		if (spanFreeList_) {
			Span* s = spanFreeList_;
			spanFreeList_ = s->next;
			return s;
		}
		if (spanPoolUsed_ < MAX_SPANS) {
			return &spanPool_[spanPoolUsed_++];
		}
		return new Span;  // fallback, should rarely happen
	}

	void freeSpanMeta(Span* s) {
		s->next = spanFreeList_;
		spanFreeList_ = s;
	}
};
