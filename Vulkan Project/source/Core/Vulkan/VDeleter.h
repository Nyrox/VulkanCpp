#pragma once
#include <Core/Definitions.h>
#include <functional>
#include <vulkan/vulkan.h>

template<typename T>
class VDeleter {
public:
	VDeleter(std::function<void(T, VkAllocationCallbacks*)> deletef);
	VDeleter(const VDeleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletf);
	VDeleter(const VDeleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef);
	~VDeleter() { cleanup(); }

	const T* operator&() const { return &object; }
	T* replace() { cleanup(); return &object; }

	operator T() const { return object; }
	void operator=(T rhs);

	template<typename V>
	bool operator==(const V rhs) { return object == T(rhs); }
private:
	T object { VK_NULL_HANDLE };
	std::function<void(T)> deleter;


	void cleanup();

};