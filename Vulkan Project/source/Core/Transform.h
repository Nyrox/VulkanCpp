#pragma once
#include <glm/glm.hpp>

struct Transform {
	explicit Transform(glm::vec3 t_position) : position(t_position) { }
	Transform() = default;

	glm::mat4 getModelMatrix() const;

	glm::vec3 position;
	glm::vec3 scale = glm::vec3(1);
};