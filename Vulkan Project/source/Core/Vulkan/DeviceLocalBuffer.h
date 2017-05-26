#pragma once
#include <Core/Definitions.h>
#include <Core/Vulkan/VulkanInstance.h>

/*
	Provides a simple to use interface with a device local buffer.
	As such it provides super high performance perfect for rarely changing vertex or index buffers.

	It however requires that the entire buffer be updated as a whole and uses a staging buffer for 
	transfering data to the gpu. As such frequent updates are gonna be slower than using a HostVisibleBuffer.
*/

class DeviceLocalBuffer {
public:
	DeviceLocalBuffer(VulkanInstance& vulkan, vk::BufferUsageFlags usageFlags);
	~DeviceLocalBuffer();

	/* Fills the current buffer. If dataSize is unequal to the current size of the buffer, it also calls this::resize. */
	void fill(void* data, uint32 dataSize);

	/* Deletes the current buffers and replaces them with new ones */
	void resize(uint32 bufferSize);


	vk::Buffer buffer;
	vk::DeviceMemory bufferMemory;
	uint32 currentBufferSize;
private:
	VulkanInstance& vulkan;
	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;
	vk::BufferUsageFlags usageFlags;

	void destroyCurrentBuffers();
};

