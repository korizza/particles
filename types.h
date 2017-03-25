#pragma once

namespace gl {

	struct vec2f {
		vec2f() : x(0.f), y(0.f) {}
		vec2f(float x_, float y_) : x(x_), y(y_) {} 
		
		float x, y;

		vec2f& operator+=(const vec2f& other) {
			x += other.x;
			y += other.y;
			return *this;
		}

		friend const vec2f operator+(const vec2f& left, const vec2f& right) {
			return vec2f(left.x+right.x, left.y+right.y);
		}

		vec2f vec2f::operator* (float right) const {
		return vec2f(x*right, y*right);
	}
	};

	struct particle {
		vec2f pos;
		vec2f speed;
		float life;
		float r, g, b, a;
		int rand;
	};
}

