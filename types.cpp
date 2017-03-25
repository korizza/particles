

#include "defines.h"
#include "types.h"
	
const gl::vec2f operator+(const gl::vec2f& left, const gl::vec2f& right) {
	return gl::vec2f(left.x+right.x, left.y+right.y);
}