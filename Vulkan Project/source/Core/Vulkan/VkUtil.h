#pragma once
#include <Core/Vulkan/VulkanInstance.h>

namespace VkUtil {
	void createBuffer(VulkanInstance&, vk::DeviceSize, vk::BufferUsageFlags, vk::MemoryPropertyFlags, vk::Buffer&, vk::DeviceMemory&);
	void createImage(VulkanInstance&, vk::Image&, vk::DeviceMemory&, vk::Extent2D, vk::Format, vk::ImageTiling, vk::ImageUsageFlags, vk::MemoryPropertyFlags);
	vk::ImageView createImageView(VulkanInstance&, vk::Image image, vk::Format format, vk::ImageAspectFlags flags);

	void copyBuffer(VulkanInstance&, vk::Buffer sourceBuffer, vk::Buffer destinationBuffer, vk::DeviceSize size);
	void copyBufferToImage(VulkanInstance&, vk::Buffer buffer, vk::Image image, vk::Extent2D);

	void transitionImageLayout(VulkanInstance&, vk::Image, vk::Format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

}


