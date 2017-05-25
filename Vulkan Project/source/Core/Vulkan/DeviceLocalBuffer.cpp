#include "DeviceLocalBuffer.h"

DeviceLocalBuffer::DeviceLocalBuffer(VulkanInstance& t_vulkan, vk::BufferUsageFlags t_flags) : vulkan(t_vulkan), usageFlags(t_flags) {

}

DeviceLocalBuffer::~DeviceLocalBuffer() {
	destroyCurrentBuffers();
}

void DeviceLocalBuffer::destroyCurrentBuffers() {
	vulkan.device.freeMemory(bufferMemory);
	vulkan.device.freeMemory(stagingBufferMemory);
	vulkan.device.destroyBuffer(buffer);
	vulkan.device.destroyBuffer(stagingBuffer);
}

void DeviceLocalBuffer::fill(void* data, uint32 dataSize) {
	if (currentBufferSize != dataSize) {
		resize(dataSize);
	}

	void* datamap = vulkan.device.mapMemory(stagingBufferMemory, 0, dataSize);
	memcpy(datamap, data, dataSize);
	vulkan.device.unmapMemory(stagingBufferMemory);

	VkUtil::copyBuffer(vulkan, stagingBuffer, buffer, dataSize);
}

void DeviceLocalBuffer::resize(uint32 bufferSize) {
	// Destroy the old buffers
	destroyCurrentBuffers();

	// Recreate the vertex and staging buffers
	VkUtil::createBuffer(vulkan, bufferSize, vk::BufferUsageFlagBits::eTransferDst | usageFlags, vk::MemoryPropertyFlagBits::eDeviceLocal, buffer, bufferMemory);
	VkUtil::createBuffer(vulkan, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

	currentBufferSize = bufferSize;
}