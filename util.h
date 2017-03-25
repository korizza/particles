#pragma once

namespace gl {

	void particle_generate(particle& p, const vec2f& v);
	void set_rndgen_seed();

	template<typename T>
	void generate_random(T from, T to, T& result) {
		result = from + static_cast <T> (rand()) / (static_cast <T> (RAND_MAX/(from - to)));
	}

}