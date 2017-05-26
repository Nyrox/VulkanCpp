#pragma once
#include <Core/Definitions.h>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

struct VulkanInstance {
	VulkanInstance();
	~VulkanInstance();

	vk::Instance instance;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::SurfaceKHR surface;
	vk::DebugReportCallbackEXT debugCallback;

	vk::Queue graphicsQueue;
	vk::Queue presentQueue;


	vk::CommandPool utilityPool;
	void createUtilityPool(uint32 queueFamilyIndex);

	vk::CommandBuffer getSingleUseCommandBuffer();
	void returnSingleUseCommandBuffer(vk::CommandBuffer commandBuffer);

	void createVulkanInstance();
	void createSurface(GLFWwindow* window);
	void createPhysicalDevice();
	void createLogicalDevice();
private:


	std::vector<const char*> getRequiredExtensions() const;
	bool checkValidationLayerSupport() const;
};