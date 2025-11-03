#pragma once

class CentralCache
{
public:
	CentralCache();
	~CentralCache();
	static CentralCache& getInstance();
	void* fetchToThreadCache(size_t);
	void reciveFromThreadCache(void* ptr, size_t);

private:
	void* fetchFromPageCache(size_t);
	void* returnToPageCache(void* ptr, size_t);
};

CentralCache::CentralCache()
{
}

CentralCache::~CentralCache()
{
}