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
	void createUtilityPool();

	vk::CommandBuffer getSingleUseCommandBuffer();
	void returnSingleUseCommandBuffer(vk::CommandBuffer commandBuffer);

	void createVulkanInstance();
	void createSurface(GLFWwindow* window);
	void createPhysicalDevice();
	void createLogicalDevice();

	struct QueueFamilyIndices {
		int graphicsFamily = -1;
		int presentFamily = -1;

		bool isComplete() { return graphicsFamily >= 0 && presentFamily >= 0; }
	};
	QueueFamilyIndices findQueueFamilyIndices(vk::PhysicalDevice device);

	struct SwapChainSupportDetails {
		vk::SurfaceCapabilitiesKHR capabilities;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR> presentModes;
	};
	SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);

private:
	

	std::vector<const char*> getRequiredExtensions() const;
	bool checkValidationLayerSupport() const;
	bool isDeviceSuitable(vk::PhysicalDevice device);
};