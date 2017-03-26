#pragma once

namespace gl {
	class spinlock {
		std::atomic_flag lockFlag;
	public:
		spinlock();
		void lock();
		void unlock();
	};

	template<typename T, unsigned int SIZE>
	class mpmc_circular_queue {
	private:
		struct node {
			T data;
			std::atomic<unsigned int> counter;
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
		std::array<node, SIZE> buffer;
		const unsigned int last_idx;
		std::atomic<unsigned int> head;
		std::atomic<unsigned int> tail;

	private:
		// hidden
		mpmc_circular_queue(mpmc_circular_queue const&);
		void operator= (mpmc_circular_queue const&);
	};
	
	class thread_joiner {
		std::vector<std::thread>& threads; 
	public:
		explicit thread_joiner(std::vector<std::thread>& threads_);
		~thread_joiner();
	};

	template<unsigned int THREAD_NUM = 2>
	class thread_pool {
		std::atomic_bool done;
		mpmc_circular_queue<std::function<void()>, THREAD_NUM> work_queue;
		//mpmc_queue<std::function<void()>> work_queue;
		std::atomic<unsigned int> oper_cntr;
		std::vector<std::thread> threads;
		thread_joiner joiner;

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

		bool in_progress() {
			unsigned int cntr = oper_cntr.load();
			return cntr != 0;
		}

		template<typename FunctionType>
		void submit(FunctionType f) {
			work_queue.push(std::function<void()>(f));
			oper_cntr.fetch_add(1);
		}
	};

}