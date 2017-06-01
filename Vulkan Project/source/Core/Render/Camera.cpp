#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(Transform t_transform, glm::mat4 t_proj) : transform(t_transform), projection(t_proj) {

}

glm::mat4 Camera::getViewMatrix() const {
	return glm::lookAt(transform.position, transform.position + forwards(), { 0, 1, 0 });
}

glm::vec3 Camera::forwards() const {
	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	return normalize(front);
}

glm::vec3 Camera::right() const {
	return glm::normalize(glm::cross(forwards(), { 0, 1, 0 }));
}