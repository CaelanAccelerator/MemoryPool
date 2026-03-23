#pragma once
#include <atomic>
#include <thread>


class SpinLockGuard
{
public:
	// Take atomic_flag by reference, not by value
	SpinLockGuard(std::atomic_flag& spinLock);
	~SpinLockGuard();

private:
	std::atomic_flag& spinLock_;
};

// Constructor now takes a reference
SpinLockGuard::SpinLockGuard(std::atomic_flag& lock)
	: spinLock_(lock)
{
	while (this->spinLock_.test_and_set(std::memory_order_acquire)) 
	{
		std::this_thread::yield();
	}
}

SpinLockGuard::~SpinLockGuard()
{
	spinLock_.clear(std::memory_order_release);
}