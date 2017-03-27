#pragma once

namespace gl {

	// lockfree spinlock
	class spinlock {
		std::atomic_flag lockFlag;
	public:
		spinlock();
		void lock();
		void unlock();
	};

	// mmpc lockfree circular queue 
	template<typename T, unsigned int SIZE>
	class mpmc_circular_queue {
	private:
		// node
		struct node {
			T data; // data
			std::atomic<unsigned int> counter; // offset ref counter
		};
	public:
		mpmc_circular_queue() : last_idx(SIZE - 1) {
			for (unsigned int i = 0; i < SIZE; ++i) {
				buffer[i].counter.store(i, std::memory_order_relaxed);
			}
			head.store(0, std::memory_order_relaxed);
			tail.store(0, std::memory_order_relaxed);
		}
		~mpmc_circular_queue() {}

		// regular push item
		bool push(T const& data) {
			node* ptr;
			unsigned int pos = 0;

			do {
				pos = tail.load(std::memory_order_relaxed);
				ptr = &buffer[pos & last_idx];
				unsigned int counter = ptr->counter.load(std::memory_order_acquire);
				long int dif = (long int)counter - (long int)pos;

				if (dif == 0) {
					if (tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
						break;
					}
				} else if (dif < 0) {
					return false;
				} else {
					pos = tail.load(std::memory_order_relaxed);
				}
			} while(true);

			ptr->data = data;
			ptr->counter.store(pos + 1, std::memory_order_release);
			return true;
		}

		// force push (in case of item with high priority
		void force_push(T const& data) {
			node* ptr;
			unsigned int pos;
  
			do {
				pos = tail.load(std::memory_order_relaxed);
				ptr = &buffer[pos & last_idx];
				unsigned int counter = ptr->counter.load(std::memory_order_acquire);
				long int dif = (long int)counter - (long int)pos;

				if (dif == 0) {
					if (tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
						break;
					}
				} else {
					unsigned int head_pos = pos - SIZE;
					if (head.compare_exchange_weak(head_pos, head_pos + 1, std::memory_order_relaxed)) {
						tail.store(pos + 1, std::memory_order_relaxed);
						break;
					}
				}
			} while (true);

			ptr->data = data;
			ptr->counter.store(pos + 1, std::memory_order_release);  
		}

		// pop item
		bool pop(T& data)
		{
			node* ptr;
			unsigned int pos = 0;

			do {
				pos = head.load(std::memory_order_relaxed);
				ptr = &buffer[pos & last_idx];
				unsigned counter = ptr->counter.load(std::memory_order_acquire);
				long int dif = (long int)counter - (long int)(pos + 1);

				if (dif == 0){
					if (head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
						break;
					}
				} else if (dif < 0) {
					return false;
				} else {
					pos = head.load(std::memory_order_relaxed);
				}
			} while (true);

			data = ptr->data;
			ptr->counter.store(pos + last_idx + 1, std::memory_order_release);
			return true;
		}

	private:
		std::array<node, SIZE> buffer; // buffer
		const unsigned int last_idx; // idx of the last element
		std::atomic<unsigned int> head; // atomic idx of the head element
		std::atomic<unsigned int> tail; // atomic idx of the tail element

	private:
		// hidden
		mpmc_circular_queue(mpmc_circular_queue const&);
		void operator= (mpmc_circular_queue const&);
	};
	
	// simple RAII for joining threads
	class thread_joiner {
		std::vector<std::thread>& threads; 
	public:
		explicit thread_joiner(std::vector<std::thread>& threads_);
		~thread_joiner();
	};

	// thread pool
	template<unsigned int THREAD_NUM = 2>
	class thread_pool {
		std::atomic_bool done; // finish job
		mpmc_circular_queue<std::function<void()>, THREAD_NUM> work_queue; // mpcp work queue
		std::atomic<unsigned int> oper_cntr; // atomic counter of queued operations
		std::vector<std::thread> threads; // threads
		thread_joiner joiner; // thread joiner

		// thread routine
		void worker_thread() {
			while(!done){
				std::function<void()> task;
				if(work_queue.pop(task)) {
					task();
					oper_cntr.fetch_sub(1);
				} else {
					std::this_thread::yield();
				}
			}
		}
	public:
		thread_pool() : joiner(threads) {
			done.store(false);
			oper_cntr.store(0);
			try {
				for(unsigned i=0; i < THREAD_NUM; ++i) {
					threads.push_back(std::move(std::thread(&thread_pool::worker_thread,this)));
				}
			} catch(...) {
				done.store(true);
				throw;
			}
		}

		~thread_pool() {
			done.store(true);
		}

		// returns true is the poool is busy
		bool in_progress() {
			unsigned int cntr = oper_cntr.load();
			return cntr != 0;
		}

		// gets a new function to the work queue
		template<typename FunctionType>
		void submit(FunctionType f) {
			work_queue.push(std::function<void()>(f));
			oper_cntr.fetch_add(1);
		}
	};

}