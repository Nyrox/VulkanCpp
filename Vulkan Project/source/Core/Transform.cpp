#include "Transform.h"
#include <glm/gtx/transform.hpp>

glm::mat4 Transform::getModelMatrix() const {
	return glm::scale(glm::translate(position), scale);
}