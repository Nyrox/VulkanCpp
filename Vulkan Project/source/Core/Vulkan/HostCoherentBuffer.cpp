#include "HostCoherentBuffer.h"
#include <Core/Vulkan/VkUtil.h>

HostCoherentBuffer::HostCoherentBuffer(VulkanInstance& t_vulkan, vk::BufferUsageFlags t_flags) : vulkan(t_vulkan), usageFlags(t_flags) {

}

HostCoherentBuffer::~HostCoherentBuffer() {
	destroyCurrentBuffers();
}

void HostCoherentBuffer::destroyCurrentBuffers() {
	vulkan.device.freeMemory(bufferMemory);
	vulkan.device.destroyBuffer(buffer);
}

void HostCoherentBuffer::fill(void* data, uint32 dataSize) {
	if (currentBufferSize != dataSize) {
		resize(dataSize);
	}

	void* datamap = vulkan.device.mapMemory(bufferMemory, 0, dataSize);
	memcpy(datamap, data, dataSize);
	vulkan.device.unmapMemory(bufferMemory);
}

void HostCoherentBuffer::resize(uint32 bufferSize) {
	// Destroy the old buffers
	destroyCurrentBuffers();

	// Recreate the vertex and staging buffers
	VkUtil::createBuffer(vulkan, bufferSize, usageFlags, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, buffer, bufferMemory);

	currentBufferSize = bufferSize;
}