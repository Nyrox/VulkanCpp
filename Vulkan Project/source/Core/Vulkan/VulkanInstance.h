#pragma once
#include <Core/Definitions.h>
#include <vulkan/vulkan.hpp>

struct VulkanInstance {
	vk::Instance instance;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::SurfaceKHR surface;


	vk::Queue graphicsQueue;
	vk::Queue presentQueue;


	vk::CommandPool utilityPool;
	void createUtilityPool(uint32 queueFamily) {
		vk::CommandPoolCreateInfo poolInfo = {};
		poolInfo.queueFamilyIndex = queueFamily;

		utilityPool = device.createCommandPool(poolInfo);
	}

	vk::CommandBuffer getSingleUseCommandBuffer() {
		vk::CommandBufferAllocateInfo allocInfo = {};
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandPool = utilityPool;
		allocInfo.commandBufferCount = 1;

		vk::CommandBuffer commandBuffer = device.allocateCommandBuffers({ allocInfo })[0];

		vk::CommandBufferBeginInfo beginInfo = {};
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		commandBuffer.begin(beginInfo);
		return commandBuffer;
	}

	void returnSingleUseCommandBuffer(vk::CommandBuffer commandBuffer) {
		commandBuffer.end();

		vk::SubmitInfo submitInfo = {};
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		graphicsQueue.submit(submitInfo, nullptr);
		graphicsQueue.waitIdle();
	
		device.freeCommandBuffers(utilityPool, 1, &commandBuffer);
	}

};