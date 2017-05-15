#include "VDeleter.h"

// Define all the function declarations explicitly for all types that can be assigned to VDeleter
// This allows us to keep the header clean
#define REGISTER_VK_OBJECT(object) \
	template VDeleter<object>::VDeleter(std::function<void(object, VkAllocationCallbacks*)>);\
	template VDeleter<object>::VDeleter(const VDeleter<VkInstance>& instance, std::function<void(VkInstance, object, VkAllocationCallbacks*)>);\
	template VDeleter<object>::VDeleter(const VDeleter<VkDevice>& device, std::function<void(VkDevice, object, VkAllocationCallbacks*)> deletef);\
	template void VDeleter<object>::cleanup();\
	template void VDeleter<object>::operator=(object rhs);\
	
REGISTER_VK_OBJECT(VkInstance)
REGISTER_VK_OBJECT(VkDebugReportCallbackEXT)
REGISTER_VK_OBJECT(VkDevice)
REGISTER_VK_OBJECT(VkSurfaceKHR)
REGISTER_VK_OBJECT(VkSwapchainKHR)
REGISTER_VK_OBJECT(VkImageView);
REGISTER_VK_OBJECT(VkShaderModule);
REGISTER_VK_OBJECT(VkRenderPass);
REGISTER_VK_OBJECT(VkPipelineLayout);
REGISTER_VK_OBJECT(VkPipeline);
REGISTER_VK_OBJECT(VkSemaphore);

// Implementation
template<typename T>
VDeleter<T>::VDeleter(std::function<void(T, VkAllocationCallbacks*)> deletef) {
	this->deleter = [=](T obj) { deletef(obj, nullptr); };
}

template<typename T>
VDeleter<T>::VDeleter(const VDeleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) {
	this->deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
}

template<typename T>
VDeleter<T>::VDeleter(const VDeleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef) {
	this->deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
}


template<typename T>
void VDeleter<T>::cleanup() {
	if (object != VK_NULL_HANDLE)
		deleter(object);

	object = VK_NULL_HANDLE;
}

template<typename T>
void VDeleter<T>::operator=(T rhs) {
	if (rhs != object) {
		cleanup();
		object = rhs;
	}
}