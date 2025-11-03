#include "../include/ThreadCache.h"

ThreadCache& ThreadCache::getinstance() {
	static ThreadCache instance;
	return instance;
}

void* ThreadCache::allocate() {
	//stub
	return nullptr;
}

void ThreadCache::deallocate() {
	//stub
}

void* ThreadCache::fetchFromCentralCache(size_t size) {
	//stub
	return nullptr;
}

void* ThreadCache::returnToCentralCache(void* ptr, size_t size) {
	//stub
	return nullptr;
}

bool ThreadCache::shouldReturn() {
	//stub
	return false;
}