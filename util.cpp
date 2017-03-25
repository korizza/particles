
#include "defines.h"
#include "types.h"
#include "config.h"

#include "util.h"

void gl::particle_generate(particle& p, const vec2f& v)
{
	// set coordinates
	p.pos.x = v.x;
	p.pos.y = v.y;

	// set speed
	generate_random(GL_PARTICLE_MIN_SPEED, GL_PARTICLE_MAX_SPEED, p.speed.x);
	generate_random(GL_PARTICLE_MIN_SPEED, GL_PARTICLE_MAX_SPEED, p.speed.y);

	// set life
	generate_random(GL_PARTICLE_MIN_LIFE_MS, GL_PARTICLE_MAX_LIFE_MS, p.life);

	// set color
	generate_random(0.f, 255.f, p.r);
	generate_random(0.f, 255.f, p.g);
	generate_random(0.f, 255.f, p.b);
	p.a = 1.f;


}
void gl::set_rndgen_seed()
{
	std::srand(std::time(nullptr));
}
