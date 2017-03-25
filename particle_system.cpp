
#include "defines.h"
#include "config.h"
#include "types.h"
#include "concurency.h"
#include "util.h"

#include "particle_system.h"


gl::particle_system::particle_system(void) {
}


gl::particle_system::~particle_system(void) {
}

particle_buffer_ptr gl::particle_system::aquireInterlockedRenderParticlesPtr(){
	renderLock.lock();
	return renderPtr;
}

void gl::particle_system::releaseInterlockedRenderParticlesPtr() {
	renderLock.unlock();
}

void gl::particle_system::generateBlast(int x, int y) {
	vec2f point;
	point.x = static_cast<float>(x);
	point.y = static_cast<float>(y);
	blastQueue.push(point);
}

void gl::particle_system::init() {
	assert(GL_PARTICLE_NUMBER_TOTAL > 254);
	assert(GL_THREAD_NUMBER > 0 && GL_THREAD_NUMBER < 4);

	updatePtr = &second;
	renderPtr = &first;

	set_rndgen_seed();

	unsigned int particalNumForThread = GL_PARTICLE_NUMBER_TOTAL / GL_THREAD_NUMBER;
	unsigned int curIdx = 0;
	for (unsigned int i = 0; i < GL_THREAD_NUMBER; ++i) {
		unsigned int endIdx = (i == GL_THREAD_NUMBER - 1) ? GL_PARTICLE_NUMBER_TOTAL : curIdx + particalNumForThread;
		taskAreas[i] = std::make_pair(curIdx, endIdx);
		curIdx += particalNumForThread;
	}

	threadPool = std::unique_ptr<thread_pool>(new thread_pool(GL_THREAD_NUMBER));
}

void gl::particle_system::fini() {
	threadPool.release();
}

void gl::particle_system::update(float delta) {
	std::array<std::unique_ptr<task>, GL_THREAD_NUMBER> taskQueue;




	for (unsigned int i = 0; i < GL_THREAD_NUMBER; ++i)	{
		std::unique_ptr<vec2f> blastPoint = blastQueue.pop();
		if (blastPoint) {
			taskQueue[i] = std::unique_ptr<task>(dynamic_cast<task*>(new blast_task(updatePtr, *blastPoint, taskAreas[i].first, taskAreas[i].second, GL_PARTICLE_NUMBER_BLAST)));
		} else {
			taskQueue[i] = std::unique_ptr<task>(dynamic_cast<task*>(new update_task(updatePtr, renderPtr, delta, taskAreas[i].first, taskAreas[i].second, &blastQueue)));		
		}
	}
	
	for (unsigned int i = 0; i < GL_THREAD_NUMBER; ++i)	{
		threadPool->submit([&taskQueue, i]() {
			taskQueue[i]->run();
		});			
	}

	while (threadPool->in_progress())	{
		std::this_thread::yield();
	}

	renderLock.lock();
	std::swap(renderPtr, updatePtr);
	renderLock.unlock();
}

void gl::blast_task::run()
{
	// populate particles
	for(unsigned int i = begin; i < end; ++i) {
		particle* p = &(*updatePtr)[i];
		if (p->life <= 0) {
			particle_generate(*p, point); 
		}
		if (--newPoints == 0) {
			break;
		}
	}

}

void gl::update_task::run()
{
	// update particles
	for(unsigned int i = begin; i < end; ++i) {
		particle& p = (*updatePtr)[i];
		
		if (p.life <= 0.f) {
			continue;
		}

		p.life -= delta;

		// todo: check out of screen particles

		if (p.life <= 0.f) {
			continue;
		}

		vec2f t1(1.f, 1.f);

		t1 += vec2f(2.f, 2.f);


		p.speed += vec2f(0.0f,-9.81f) * delta;
		p.pos += p.speed * delta;
	}
}