#pragma once
#include <Core/Transform.h>

class Camera {
public:
	Camera(Transform transform, glm::mat4 projection);

	glm::mat4 getViewMatrix() const;
	glm::vec3 forwards() const;
	glm::vec3 right() const;

	Transform transform;
	glm::mat4 projection;
	float yaw = 0, pitch = 0;
private:

};