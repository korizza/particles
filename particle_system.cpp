
#include "defines.h"
#include "config.h"
#include "types.h"
#include "concurency.h"

#include "particle_system.h"


gl::particle_system::particle_system(void) {
}


gl::particle_system::~particle_system(void) {
}

void gl::particle_system::init_particle_arrays() {		
	std::srand(std::time(nullptr));

	for (unsigned int i = 0; i < GL_PARTICLE_NUMBER_TOTAL; i++) {
		particle p;

		// set initial life life (dead)
		p.life = -1.f;

		// set random seed
		p.rand = std::rand();

		// set speed
		float angle, speed;
		generate_random(0.f, 360.f, angle, p.rand);
		generate_random(GL_PARTICLE_MIN_SPEED, GL_PARTICLE_MAX_SPEED, speed, p.rand);
		p.speed.x = speed*std::cos(angle);
		p.speed.y = speed*std::sin(angle);
		p.r = p.g = p.b = p.a = 255.f;

		first[i] = second[i] = p;
	}
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
	point.y = static_cast<float>(scrHeight - y);
	blastQueue.force_push(point);
}

void gl::particle_system::init(float scrWidth_, float scrHeight_) {

	hasExit = false;

	assert(GL_PARTICLE_NUMBER_TOTAL > 254);

	scrHeight = scrHeight_;
	scrWidth = scrWidth_;
	deltaDelay = 0;

	init_particle_arrays();

	unsigned int blastPerArea = GL_PARTICLE_NUMBER_BLAST / GL_THREAD_NUMBER, blastCntr = GL_PARTICLE_NUMBER_BLAST;
	assert(blastPerArea > 1);

	updatePtr = &second;
	renderPtr = &first;
	
	unsigned int particalNumForThread = GL_PARTICLE_NUMBER_TOTAL / GL_THREAD_NUMBER;
	unsigned int curIdx = 0;
	for (unsigned int i = 0; i < GL_THREAD_NUMBER; ++i) {
		unsigned int endIdx = (i == GL_THREAD_NUMBER - 1) ? GL_PARTICLE_NUMBER_TOTAL : curIdx + particalNumForThread;
		taskAreas[i] = std::make_pair(curIdx, endIdx);
		curIdx += particalNumForThread;
		blastPerAreaArr[i] = (i == (GL_THREAD_NUMBER - 1) ) ? blastCntr : blastPerArea;
		blastCntr -= blastPerArea;
	}

	threadPool = std::unique_ptr<thread_pool<GL_THREAD_NUMBER>>(new thread_pool<GL_THREAD_NUMBER>());
}

void gl::particle_system::fini() {
	hasExit = true;

	while (threadPool->in_progress())	{
		std::this_thread::yield();
	}

	threadPool.release();
}

void gl::particle_system::update(float delta) {
	if (hasExit) {
		return;
	}

	std::array<std::unique_ptr<task>, GL_THREAD_NUMBER> taskQueue;
	
	vec2f blastPoint;
	if (blastQueue.pop(blastPoint) && (deltaDelay < GL_MAX_DELTA_DELAY)) {
		deltaDelay += delta;
		for (unsigned int i = 0; i < GL_THREAD_NUMBER; ++i) {
			taskQueue[i] = std::unique_ptr<task>(dynamic_cast<task*>(new blast_task(*this, blastPoint, taskAreas[i].first, taskAreas[i].second, blastPerAreaArr[i])));
		}
	} else {
		for (unsigned int i = 0; i < GL_THREAD_NUMBER; ++i) {
			taskQueue[i] = std::unique_ptr<task>(dynamic_cast<task*>(new update_task(*this, delta+deltaDelay, taskAreas[i].first, taskAreas[i].second)));
		}
		deltaDelay = 0.f;
	}

	for (unsigned int i = 0; i < GL_THREAD_NUMBER; ++i)	{
		threadPool->submit([&taskQueue, i]() {
			taskQueue[i]->run();
		});			
	}

	while (threadPool->in_progress())	{
		std::this_thread::yield();
	}

	// swap pointers
	renderLock.lock();
	std::swap(renderPtr, updatePtr);
	renderLock.unlock();

	// copy origin data to update array 
	std::copy(renderPtr->begin(), renderPtr->end(), updatePtr->begin());
}

void gl::blast_task::run()
{
	for(unsigned int i = begin; i < end; ++i) {
		//(*ps.updatePtr)[i] = (*ps.renderPtr)[i];
		particle& up = (*ps.updatePtr)[i];
		if (up.life < 0) {
			particle_generate(up, point);
			if (--newPoints == 0) {
				break;
			}
		}
	}
}

void gl::update_task::run()
{
	// update particles
	for(unsigned int i = begin; i < end; ++i) {
		//(*ps.updatePtr)[i] = (*ps.renderPtr)[i];
		particle& up = (*ps.updatePtr)[i];
		
		if (up.life < 0.f) {
			continue;
		}

		if ((up.pos.x > ps.scrWidth) || (up.pos.x < 0.f) || (up.pos.y < 0.f) || (up.pos.y > ps.scrHeight)) {
			up.life = -1.f;
			continue;
		}

		up.life -= delta;
		if (up.life < 0.f ) {
			if (probability_perc(GL_PARTICLE_BLAST_CHANCE_PERC, std::rand())) {
				vec2f point;
				point.x = up.pos.x;
				point.y = up.pos.y;
				ps.blastQueue.push(point);
			}
			continue;
		}

		up.pos += up.speed * delta;
	}
}

void gl::task::particle_generate(particle& p, const vec2f& v)
{
	// set coordinates
	p.pos.x = v.x;
	p.pos.y = v.y;

	// set life
	generate_random(GL_PARTICLE_MIN_LIFE_MS, GL_PARTICLE_MAX_LIFE_MS, p.life, p.rand);
}

bool gl::probability_perc(int perc, int rnd) {
	return (rnd % 100) < perc;
}