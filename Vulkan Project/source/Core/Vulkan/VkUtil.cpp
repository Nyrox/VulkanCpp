#include "VkUtil.h"
#include <Core/Definitions.h>

namespace VkUtil {
	namespace {
		uint32 findMemoryType(vk::PhysicalDevice pDevice, uint32 typeFilter, vk::MemoryPropertyFlags properties) {
			vk::PhysicalDeviceMemoryProperties memProperties = pDevice.getMemoryProperties();

			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
				if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					return i;
				}
			}

			throw std::runtime_error("failed to find suitable memory type!");
		}
	}

	void createBuffer(VulkanInstance& vulkan, vk::DeviceSize bufferSize, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& memory) {
		vk::BufferCreateInfo bufferInfo = {};
		bufferInfo.size = bufferSize;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
	
		buffer = vulkan.device.createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequirements = vulkan.device.getBufferMemoryRequirements(buffer);
		vk::MemoryAllocateInfo allocInfo = {};
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(vulkan.physicalDevice, memoryRequirements.memoryTypeBits, properties);

		memory = vulkan.device.allocateMemory(allocInfo);
		vulkan.device.bindBufferMemory(buffer, memory, 0);
	}

	void createImage(VulkanInstance& vulkan, vk::Image& image, vk::DeviceMemory& memory, vk::Extent2D extent, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) {
		vk::ImageCreateInfo imageInfo = {};
		imageInfo.imageType = vk::ImageType::e2D;
		imageInfo.extent = { (uint32)extent.width, (uint32)extent.height, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = vk::ImageLayout::ePreinitialized;
		imageInfo.usage = usage;
		imageInfo.sharingMode = vk::SharingMode::eExclusive;
		imageInfo.samples = vk::SampleCountFlagBits::e1;

		image = vulkan.device.createImage(imageInfo);

		vk::MemoryRequirements memoryRequirements = vulkan.device.getImageMemoryRequirements(image);
		vk::MemoryAllocateInfo allocInfo = {};
		allocInfo.allocationSize = memoryRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(vulkan.physicalDevice, memoryRequirements.memoryTypeBits, properties);

		memory = vulkan.device.allocateMemory(allocInfo);
		vulkan.device.bindImageMemory(image, memory, 0);
	}

	vk::ImageView createImageView(VulkanInstance& vulkan, vk::Image image, vk::Format format, vk::ImageAspectFlags flags) {
		vk::ImageViewCreateInfo viewInfo = {};
		viewInfo.image = image;
		viewInfo.viewType = vk::ImageViewType::e2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = flags;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;

		return vulkan.device.createImageView(viewInfo);
	}

	void copyBuffer(VulkanInstance& vulkan, vk::Buffer sourceBuffer, vk::Buffer destinationBuffer, vk::DeviceSize size) {
		auto commandBuffer = vulkan.getSingleUseCommandBuffer();

		vk::BufferCopy copyRegion = {};
		copyRegion.size = size;
		commandBuffer.copyBuffer(sourceBuffer, destinationBuffer, copyRegion);

		vulkan.returnSingleUseCommandBuffer(commandBuffer);
	}

	void copyBufferToImage(VulkanInstance& vulkan, vk::Buffer buffer, vk::Image image, vk::Extent2D extent) {
		auto commandBuffer = vulkan.getSingleUseCommandBuffer();

		vk::BufferImageCopy region = {};
		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.layerCount = 1;
		region.imageExtent = { extent.width, extent.height, 1 };
		
		commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);
		vulkan.returnSingleUseCommandBuffer(commandBuffer);
	}

	void transitionImageLayout(VulkanInstance& vulkan, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
		auto commandBuffer = vulkan.getSingleUseCommandBuffer();
	
		vk::ImageMemoryBarrier barrier = {};
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.image = image;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.layerCount = 1;

		if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
		else barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;


		if (oldLayout == vk::ImageLayout::ePreinitialized && newLayout == vk::ImageLayout::eTransferDstOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		}
		else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits(0);
			barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &barrier);
		vulkan.returnSingleUseCommandBuffer(commandBuffer);
	}

}