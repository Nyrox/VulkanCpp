#include "Window.h"

Window::Window(uint32 w, uint32 h, std::string t) {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	nativeHandle = glfwCreateWindow(w, h, t.c_str(), nullptr, nullptr);
	
}

Window::~Window() {
	glfwDestroyWindow(nativeHandle);
	glfwTerminate();
}

bool Window::isOpen() const {
	return !glfwWindowShouldClose(nativeHandle);
}

void Window::close() const {
	glfwSetWindowShouldClose(nativeHandle, true);
}

