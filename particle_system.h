#pragma once

typedef std::array<gl::particle, GL_PARTICLE_NUMBER_TOTAL> particle_buffer, *particle_buffer_ptr;
typedef std::array<std::pair<unsigned int, unsigned int>, GL_THREAD_NUMBER> task_areas;
typedef gl::mpmc_queue<gl::vec2f> blast_queue;

namespace gl {
	class particle_system
	{
	public:
		particle_system(void);
		~particle_system(void);

		void init(); 
		void fini();
		void update(float delta);

		particle_buffer_ptr aquireInterlockedRenderParticlesPtr();
		void releaseInterlockedRenderParticlesPtr();

		void generateBlast(int x, int y);

	private:


	private:
		volatile bool hasExit;

		particle_buffer first, second;		
		particle_buffer_ptr updatePtr, renderPtr;

		spinlock renderLock;

		std::unique_ptr<thread_pool> threadPool;
		mpmc_queue<vec2f> blastQueue;
		task_areas taskAreas;
	};

	class task {
	public:
		virtual void run()=0;
		virtual ~task(){}
	protected:
		unsigned int begin, end;
		particle_buffer_ptr updatePtr;
	};

	class update_task : public task {
	public:
		update_task(particle_buffer_ptr updatePtr_, particle_buffer_ptr renderPtr_, float delta_, unsigned int begin_, unsigned int end_, blast_queue* blastQueue_) :
			renderPtr(renderPtr_), delta(delta_), blastQueue(blastQueue_)  { begin = begin_; end = end_; updatePtr = updatePtr_; }
		virtual ~update_task() {}
		virtual void run();
	private:
		particle_buffer_ptr renderPtr;
		float delta;

		blast_queue*  blastQueue;
	};

	class blast_task : public task {
	public:
		blast_task(particle_buffer_ptr updatePtr_, gl::vec2f point_, unsigned int begin_, unsigned int end_, unsigned int newPoints_) : 
			point(point_), newPoints(newPoints_)  { begin = begin_; end = end_; updatePtr = updatePtr_; }
		virtual ~blast_task() {}
		virtual void run();
	private:
		vec2f point;
		unsigned int newPoints;
	};


}