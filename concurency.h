﻿#pragma once

namespace gl {
	class spinlock {
		std::atomic_flag lockFlag;
	public:
		spinlock();
		void lock();
		void unlock();
	};

	template <typename T>
	class mpmc_queue {
	  private:
		struct node;
		struct counted_node_ptr {
			int external_count;
			node* ptr;
		};

		std::atomic<counted_node_ptr> head;
		std::atomic<counted_node_ptr> tail;

		struct node_counter {
			unsigned internal_count:30;
			unsigned external_counters:2;
		};

		struct node {
			std::atomic<T*> data;
			std::atomic<node_counter> count;
			std::atomic<counted_node_ptr> next;

			node() {
				node_counter new_count;
				new_count.internal_count = 0;
				new_count.external_counters = 2; 
				count.store(new_count);

				counted_node_ptr new_next;
				new_next.ptr = nullptr;
				new_next.external_count = 0;
				next.store(new_next);
			}

			void release_ref() {
				node_counter old_counter = count.load(std::memory_order_relaxed);
				node_counter new_counter;
				do {
					new_counter = old_counter;
					--new_counter.internal_count;
				} while (!count.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed));
	
				if (!new_counter.internal_count && !new_counter.external_counters)	{
					delete this;
				}
			}
		};

		void set_new_tail(counted_node_ptr &old_tail, counted_node_ptr const &new_tail) {
			node* const current_tail_ptr = old_tail.ptr;
			while(!tail.compare_exchange_weak(old_tail,new_tail) && old_tail.ptr == current_tail_ptr);
			if(old_tail.ptr == current_tail_ptr) {
				free_external_counter(old_tail);
			} else {
				current_tail_ptr->release_ref();
			}
		}

		static void increase_external_count(std::atomic<counted_node_ptr>& counter, counted_node_ptr& old_counter) {
			counted_node_ptr new_counter;
			do {
				new_counter = old_counter;
				++new_counter.external_count;
			}

			while(!counter.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire,std::memory_order_relaxed));
			old_counter.external_count = new_counter.external_count;
		}

		static void free_external_counter(counted_node_ptr &old_node_ptr) {
			node* const ptr = old_node_ptr.ptr;
			int const count_increase = old_node_ptr.external_count-2;
			node_counter old_counter = ptr->count.load(std::memory_order_relaxed);
			node_counter new_counter;

			do {
				new_counter=old_counter;
				--new_counter.external_counters;
				new_counter.internal_count+=count_increase;
			}

			while(!ptr->count.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire,std::memory_order_relaxed));
	
			if(!new_counter.internal_count && !new_counter.external_counters) {
				delete ptr;
			}
		}

		// hidden
		mpmc_queue(const mpmc_queue& other) {};
		mpmc_queue& operator=(const mpmc_queue& other) {};

	  public:
		mpmc_queue() {
			counted_node_ptr cntr;
			cntr.external_count = 0;
			cntr.ptr = new node;
			head.store(cntr);
			tail.store(head);
		}
		~mpmc_queue() {
			while (pop());
			counted_node_ptr cntr = head.load();
			delete cntr.ptr;
		}

		void push(T new_value) {
			std::unique_ptr<T> new_data(new T(new_value));
			counted_node_ptr new_next;
			new_next.ptr = new node;
			new_next.external_count=1;
			counted_node_ptr old_tail=tail.load();

			for(;;) {
				increase_external_count(tail,old_tail);
				T* old_data=nullptr;
				if(old_tail.ptr->data.compare_exchange_strong(old_data,new_data.get())) {
					counted_node_ptr old_next={0};
					if(!old_tail.ptr->next.compare_exchange_strong(old_next,new_next)) {
						delete new_next.ptr;
						new_next=old_next;
					}
					set_new_tail(old_tail, new_next);
					new_data.release();
					break;
				} else {
					counted_node_ptr old_next={0};
					if(old_tail.ptr->next.compare_exchange_strong(old_next,new_next)) {
						old_next=new_next;
						new_next.ptr=new node;
					}
					set_new_tail(old_tail, old_next);
				}
			}
		}

		std::unique_ptr<T> pop() {
			counted_node_ptr old_head = head.load(std::memory_order_relaxed);
			for(;;) {
				increase_external_count(head, old_head);

				node* const ptr = old_head.ptr;
				if (ptr==tail.load().ptr) {
					ptr->release_ref();
					return std::unique_ptr<T>();
				}

				counted_node_ptr next = ptr->next.load();

				if (head.compare_exchange_strong(old_head, next)) {
					T* const res=ptr->data.exchange(nullptr);
					free_external_counter(old_head);
					return std::unique_ptr<T>(res);
				}
				ptr->release_ref();
			}
		}
	};

	class thread_joiner {
		std::vector<std::thread>& threads; 
	public:
		explicit thread_joiner(std::vector<std::thread>& threads_);
		~thread_joiner();
	};

	class thread_pool
	{
		std::atomic_bool done;
		mpmc_queue<std::function<void()>> work_queue;
		std::atomic<unsigned int> oper_cntr;
		std::vector<std::thread> threads;
		thread_joiner joiner;

		void worker_thread() {
			while(!done){
				auto task = work_queue.pop();
				if(task) {
					(*task)();
					oper_cntr.fetch_sub(1);
				} else {
					std::this_thread::yield();
				}
			}
		}
	public:
		thread_pool(unsigned int thread_num) : joiner(threads) {
			done.store(false);
			oper_cntr.store(0);
			try {
				for(unsigned i=0; i < thread_num; ++i) {
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