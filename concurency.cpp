
#include "defines.h"
#include "concurency.h"

gl::spinlock::spinlock()
{
	lockFlag.clear();
}

void gl::spinlock::lock()
{
	while (lockFlag.test_and_set(std::memory_order_acquire)) { ; }
}

void gl::spinlock::unlock()
{
	lockFlag.clear(std::memory_order_release);
}

gl::thread_joiner::thread_joiner(std::vector<std::thread>& threads_) : threads(threads_) {}

gl::thread_joiner::~thread_joiner()
{
	for(unsigned long i=0;i<threads.size();++i)	{
		if(threads[i].joinable()) {
			threads[i].join();
		}
	}
}



