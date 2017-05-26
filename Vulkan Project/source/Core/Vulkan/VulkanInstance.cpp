#include "VulkanInstance.h"
#include <GLFW/glfw3.h>

/* Application metadata */
constexpr auto APPLICATION_NAME = "Praise kek";
constexpr auto APPLICATION_VERSION = VK_MAKE_VERSION(1, 0, 0);
constexpr auto ENGINE_NAME = "No engine";
constexpr auto ENGINE_VERSION = VK_MAKE_VERSION(1, 0, 0);
constexpr auto VULKAN_API_VERSION = VK_API_VERSION_1_0;

/* Enable validation layers? */
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

/* Validation layers to enable */
const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

/* Device extensions to load */
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


// 
std::vector<const char*> VulkanInstance::getRequiredExtensions() const {
	std::vector<const char*> extensions;

	uint32 glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (unsigned int i = 0; i < glfwExtensionCount; i++) {
		extensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	return extensions;
}

//
bool VulkanInstance::checkValidationLayerSupport() const {
	auto availableLayers = vk::enumerateInstanceLayerProperties();

	for (auto layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) return false;
	}
	
	return true;
}

VulkanInstance::VulkanInstance() {

}

VulkanInstance::~VulkanInstance() {
	instance.destroyDebugReportCallbackEXT(debugCallback);
	instance.destroy();
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL vulkanDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64 obj, size_t location, int32 code, const char* layerPrefix, const char* msg, void* userData) {

	std::cerr << "Validation layer: " << msg << std::endl;

	return VK_FALSE;
}

void VulkanInstance::createVulkanInstance() {
	vk::ApplicationInfo appInfo(APPLICATION_NAME, APPLICATION_VERSION, ENGINE_NAME, ENGINE_VERSION, VULKAN_API_VERSION);
	vk::InstanceCreateInfo createInfo;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

	// Double check for validation layer support
	if (enableValidationLayers && !checkValidationLayerSupport())
		throw std::runtime_error("Validation layers requested, but not vailable! Try installing the Lunar Vulkan SDK.");

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}

	instance = vk::createInstance(createInfo);

	vk::DebugReportCallbackCreateInfoEXT callbackInfo(vk::DebugReportFlagsEXT(), vulkanDebugCallback);
	debugCallback = instance.createDebugReportCallbackEXT(callbackInfo);
}

void VulkanInstance::createSurface(GLFWwindow* window) {
	if (glfwCreateWindowSurface(static_cast<VkInstance>(instance), window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}
}



void VulkanInstance::createUtilityPool(uint32 queueFamily) {
	vk::CommandPoolCreateInfo poolInfo = {};
	poolInfo.queueFamilyIndex = queueFamily;

	utilityPool = device.createCommandPool(poolInfo);
}





vk::CommandBuffer VulkanInstance::getSingleUseCommandBuffer() {
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

void VulkanInstance::returnSingleUseCommandBuffer(vk::CommandBuffer commandBuffer) {
	commandBuffer.end();

	vk::SubmitInfo submitInfo = {};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	graphicsQueue.submit(submitInfo, nullptr);
	graphicsQueue.waitIdle();

	device.freeCommandBuffers(utilityPool, 1, &commandBuffer);
}