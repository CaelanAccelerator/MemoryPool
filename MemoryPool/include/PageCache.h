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
	std::map<void*, Span*> spanMap_; // a mapping from address to Span, used for recycle
	std::mutex mutexLock;
};