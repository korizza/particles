
#include "defines.h"

#include "./nvToolsExt.h"
#include "types.h"
#include "config.h"

#include "concurency.h"
#include "particle_system.h"
#include "test.h"
	



// Just some hints on implementation
// You could remove all of them

static std::atomic<float> globalTime;
static volatile bool workerMustExit = false;
gl::particle_system particleSystem;


// some code

void WorkerThread(void)
{
	while (!workerMustExit)
	{
		nvtxRangePush(__FUNCTION__);

		static float lastTime = 0.f;
		float time = globalTime.load();
		float delta = time - lastTime;
		lastTime = time;

		if (delta > 0) particleSystem.update(delta);
		
		if (delta < 10)
			std::this_thread::sleep_for(std::chrono::milliseconds(10 - static_cast<int>(delta*1000.f)));

		nvtxRangePop();
	}
}

void test::init(void)
{
	// some code
	particleSystem.init();

	std::thread workerThread(WorkerThread);
	workerThread.detach(); // Glut + MSVC = join hangs in atexit()

	// some code
}

void test::term(void)
{
	// some code

	workerMustExit = true;
	particleSystem.fini();
	// some code
}

void test::render(void)
{
	auto renderPtr = particleSystem.aquireInterlockedRenderParticlesPtr();
	for (auto it = renderPtr->begin(); it != renderPtr->end(); ++it) {
		if (it->life > 0.f) {
			platform::drawPoint(it->pos.x, it->pos.y, 255.f, 255.f, 255.f, 255.f);
			//platform::drawPoint(it->pos.x, it->pos.y, it->r, it->g, it->b, it->a);
		}
	}
	particleSystem.releaseInterlockedRenderParticlesPtr();
}

void test::update(int dt)
{
	// some code

	float time = globalTime.load();
	globalTime.store(time + dt);

	// some code
}

void test::on_click(int x, int y)
{
	particleSystem.generateBlast(x, y);
}