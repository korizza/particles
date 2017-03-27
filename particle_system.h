#pragma once

typedef std::array<gl::particle, GL_PARTICLE_NUMBER_TOTAL> particle_buffer, *particle_buffer_ptr;
typedef std::array<std::pair<unsigned int, unsigned int>, GL_THREAD_NUMBER> task_areas;
typedef gl::mpmc_circular_queue<gl::vec2f, GL_PARTICLE_EFFECTS_TOTAL> blast_queue;

namespace gl {
	class blast_task;
	class update_task;

	// implements a simple partical system based on a lockfree thread pool
	class particle_system
	{
	public:
		particle_system();
		~particle_system();

		// initialize
		void init(float scrWidth_, float scrHeight_); 
		
		// finilize
		void fini();

		// update operation
		void update(float delta);

		// aquires a spinlock and returns a pointer to the buffer of rendered particles
		particle_buffer_ptr aquireInterlockedRenderParticlesPtr();

		// releases render buffer spinlock
		void releaseInterlockedRenderParticlesPtr();

		// generates a boom! task
		void generateBlast(int x, int y);

	private:
		// initializes particle arrays
		void init_particle_arrays();
	private:
		float scrWidth, scrHeight; // screen width/height
		particle_buffer first, second;	// particle buffers	
		particle_buffer_ptr updatePtr, renderPtr; // pointers to the particle buffers
				
		spinlock renderLock; // simple lockfree spinlock

		std::unique_ptr<thread_pool<GL_THREAD_NUMBER>> threadPool; // thread pool
		blast_queue blastQueue; // queue of boom! tasks
		task_areas taskAreas; // array of buffer areas. depends on number of threads
		std::array<unsigned int, GL_THREAD_NUMBER> blastPerAreaArr; // shares blasts between threads
		float deltaDelay; // counts deltas while system are processing blast tasks
		volatile bool hasExit; // exit variable

		friend class blast_task;
		friend class update_task;
	};

	// absract task
	class task {
	public:
		// this functions should be called from working thread
		virtual void run()=0;
		virtual ~task(){}
	protected:
		// generates a new blast of particles
		void particle_generate(particle& p, const vec2f& v);

	protected:
		unsigned int begin, end; // interval
	};

	// implements update task
	class update_task : public task {
	public:
		update_task(particle_system& ps_, float delta_, unsigned int begin_, unsigned int end_) : delta(delta_), ps(ps_) { 
			begin = begin_; 
			end = end_; 
		}
		virtual ~update_task() {}
		// override
		virtual void run();
	private:
		float delta; // delta
		particle_system& ps; // reference to caller
	};

	// implements blast task
	class blast_task : public task {
	public:
		blast_task(particle_system& ps_, const gl::vec2f& point_, unsigned int begin_, unsigned int end_, unsigned int newPoints_) : point(point_), newPoints(newPoints_), ps(ps_) { 
			begin = begin_; 
			end = end_; 
		}
		virtual ~blast_task() {}

		// override
		virtual void run();

	private:
		vec2f point; // point
		unsigned int newPoints; // number of particles to be generated
		particle_system& ps; // reference to caller
	};

		
	// generates random number  
	template<typename T>
	void generate_random(T from, T to, T& result, int rnd) {
		result = std::abs(from + static_cast <T> (rnd) / (static_cast <T> (RAND_MAX/(from - to))));		
	}

	// generates random number with defined probability
	bool probability_perc(int perc, int rnd);
}