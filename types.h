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

		friend const vec2f operator+(const vec2f& left, const vec2f& right); 
		
		template<typename T>
		friend const vec2f operator*(const vec2f& left, T& right);
	};

	struct particle {
		particle() : life(-1.f) {}

		vec2f pos;
		vec2f speed;
		float life;
		float weight;
		float angle;
		float r, g, b, a;
	};
	
	const vec2f operator+(const vec2f& left, const vec2f& right);

	template<typename T>
	const vec2f operator*(const vec2f& left, T& right) {
		return vec2f(left.x+static_cast<float>(right), left.y+static_cast<float>(right));		
	}



}

