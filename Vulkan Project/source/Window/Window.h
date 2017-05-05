#pragma once
#include <Core/Definitions.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


class Window {
public:
	Window(uint32 w, uint32 h, std::string t);
	Window(Window&&) = default;
	~Window();

	bool isOpen() const;
	void close() const;

	GLFWwindow* nativeHandle = nullptr;
private:
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
};