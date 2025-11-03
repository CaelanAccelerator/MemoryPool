#include "../include/CentralCache.h"


CentralCache& CentralCache::getInstance() {
	static CentralCache instance;
	return instance;
}

void* CentralCache::fetchToThreadCache(size_t size) {
	// stub
	return nullptr;
}

void* CentralCache::fetchFromPageCache(size_t size) {
	// stub
	return nullptr;
}

void* CentralCache::returnToPageCache(void* ptr, size_t size) {
	// stub
	return nullptr;
}

void CentralCache::reciveFromThreadCache(void* ptr, size_t size) {
	// stub
}
