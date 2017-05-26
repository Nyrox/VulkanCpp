#pragma once
#include <Core/Definitions.h>
#include <Core/Vulkan/VulkanInstance.h>


/*
Provides a simple to use interface with a host coherent buffer.
As such it provides decent read and excellent write performance perfect for requently changing vertex or uniform buffers.

It however requires that the entire buffer be updated as a whole and uses less efficient memory on the gpu
as such it is slower to access than a DeviceLocalBuffer.
*/

class HostCoherentBuffer {
public:
	HostCoherentBuffer(VulkanInstance& vulkan, vk::BufferUsageFlags usageFlags);
	~HostCoherentBuffer();

	/* Fills the current buffer. If dataSize is unequal to the current size of the buffer, it also calls this::resize. */
	void fill(void* data, uint32 dataSize);

	/* Deletes the current buffer and replaces it */
	void resize(uint32 bufferSize);


	vk::Buffer buffer;
	vk::DeviceMemory bufferMemory;
	uint32 currentBufferSize;
private:
	VulkanInstance& vulkan;
	vk::BufferUsageFlags usageFlags;

	void destroyCurrentBuffers();
};
