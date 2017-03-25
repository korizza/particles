#pragma once

typedef std::array<gl::particle, GL_PARTICLE_NUMBER_TOTAL> particle_buffer, *particle_buffer_ptr;
typedef std::array<std::pair<unsigned int, unsigned int>, GL_THREAD_NUMBER> task_areas;
typedef gl::mpmc_queue<gl::vec2f> blast_queue;

namespace gl {
	template<typename T>
	void generate_random(T from, T to, T& result, int rnd) {
		result = std::abs(from + static_cast <T> (rnd) / (static_cast <T> (RAND_MAX/(from - to))));		
	}

	class blast_task;
	class update_task;

	class particle_system
	{
	public:
		particle_system(void);
		~particle_system(void);

		void init(float scrWidth_, float scrHeight_); 
		void fini();
		void update(float delta);

		particle_buffer_ptr aquireInterlockedRenderParticlesPtr();
		void releaseInterlockedRenderParticlesPtr();

		void generateBlast(int x, int y);

	private:
		void init_particle_arrays();
	private:
		float scrWidth, scrHeight;
		particle_buffer first, second;		
		particle_buffer_ptr updatePtr, renderPtr;

		spinlock renderLock;

		std::unique_ptr<thread_pool> threadPool;
		mpmc_queue<vec2f> blastQueue;
		task_areas taskAreas;
		std::array<unsigned int, GL_THREAD_NUMBER> blastPerAreaArr;
		float deltaDelay;


		friend class blast_task;
		friend class update_task;
	};

	class task {
	public:
		virtual void run()=0;
		virtual ~task(){}
	protected:
		void particle_generate(particle& p, const vec2f& v);
		bool probability_perc(int perc, int rnd);

	protected:
		unsigned int begin, end;
	};

	class update_task : public task {
	public:
		update_task(particle_system& ps_, float delta_, unsigned int begin_, unsigned int end_) : delta(delta_), ps(ps_) { 
			begin = begin_; 
			end = end_; 
		}
		virtual ~update_task() {}
		virtual void run();
	private:
		float delta;
		particle_system& ps;
	};

	class blast_task : public task {
	public:
		blast_task(particle_system& ps_, gl::vec2f point_, unsigned int begin_, unsigned int end_, unsigned int newPoints_) : point(point_), newPoints(newPoints_), ps(ps_) { 
			begin = begin_; 
			end = end_; 
		}
		virtual ~blast_task() {}
		virtual void run();
	private:
		vec2f point;
		unsigned int newPoints;
		particle_system& ps;
	};


}