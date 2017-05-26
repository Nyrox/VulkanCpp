#include "VulkanInstance.h"
#include <GLFW/glfw3.h>
#include <set>

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

	vk::DebugReportCallbackCreateInfoEXT callbackInfo(vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning, vulkanDebugCallback);
	debugCallback = instance.createDebugReportCallbackEXT(callbackInfo);
}

void VulkanInstance::createSurface(GLFWwindow* window) {
	if (glfwCreateWindowSurface(static_cast<VkInstance>(instance), window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}
}

VulkanInstance::QueueFamilyIndices VulkanInstance::findQueueFamilyIndices(vk::PhysicalDevice device) {
	QueueFamilyIndices indices;
	auto queueFamilies = device.getQueueFamilyProperties();

	for (int i = 0; i < queueFamilies.size(); i++) {
		auto& queueFamily = queueFamilies.at(i);

		if (queueFamily.queueCount <= 0) continue;

		if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) indices.graphicsFamily = i;
		if (device.getSurfaceSupportKHR(i, surface)) indices.presentFamily = i;

		if (indices.isComplete()) break;
	}

	return indices;
}

VulkanInstance::SwapChainSupportDetails VulkanInstance::querySwapChainSupport(vk::PhysicalDevice device) {
	SwapChainSupportDetails details;
	details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
	details.formats = device.getSurfaceFormatsKHR(surface);
	details.presentModes = device.getSurfacePresentModesKHR(surface);

	return details;
}

bool VulkanInstance::isDeviceSuitable(vk::PhysicalDevice device) {
	auto indices = findQueueFamilyIndices(device);

	auto checkDeviceExtensionSupport = [&]() -> bool {
		auto availableExtensions = device.enumerateDeviceExtensionProperties();

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	};

	auto checkSwapChainSupport = [&]() -> bool {
		auto swapChainSupport = querySwapChainSupport(device);
		return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	};

	vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();

	return indices.isComplete() && checkDeviceExtensionSupport() && checkSwapChainSupport()
		&& (supportedFeatures.samplerAnisotropy);
}

void VulkanInstance::createPhysicalDevice() {
	auto devices = instance.enumeratePhysicalDevices();

	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("Failed to find a suitable physical device.");
	}
}

void VulkanInstance::createLogicalDevice() {
	auto indices = findQueueFamilyIndices(physicalDevice);

	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<int32> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

	float queuePriority = 1.0f;
	for (auto queueFamily : uniqueQueueFamilies) {
		vk::DeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// Insert any dank features here
	vk::PhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = true;

	vk::DeviceCreateInfo createInfo = {};
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = queueCreateInfos.size();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = deviceExtensions.size();
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// Enable validation layers
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}

	device = physicalDevice.createDevice(createInfo);
	graphicsQueue = device.getQueue(indices.graphicsFamily, 0);
	presentQueue = device.getQueue(indices.presentFamily, 0);
}

void VulkanInstance::createUtilityPool() {
	vk::CommandPoolCreateInfo poolInfo = {};
	poolInfo.queueFamilyIndex = findQueueFamilyIndices(physicalDevice).graphicsFamily;

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