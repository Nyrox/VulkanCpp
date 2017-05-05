#include <Window/Window.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>

#include <Core/Vulkan/VDeleter.h>
#include <Core/Vulkan/VDebug.h>
#include <set>
#include <algorithm>

const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation",
	//"VK_LAYER_LUNARG_api_dump"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType,
	uint64 obj, size_t location, int32 code, const char* layerPrefix, const char* msg, void* userData) {

	std::cerr << "Validation layer: " << msg << std::endl;

	return VK_FALSE;
}

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};


SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice pDevice, VkSurfaceKHR surface);


bool checkValidationLayerSupport() {
	uint32 layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());


	for (const char* layerName : validationLayers) {
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
std::vector<const char*> getRequiredExtensions() {
	std::vector<const char*> extensions;

	uint32 glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (unsigned int i = 0; i < glfwExtensionCount; i++) {
		extensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	return extensions;
}

void createVulkanInstance(VDeleter<VkInstance>& instance) {
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Query supported extensions and output them to the screen
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::cout << "Available extensions: " << "\n";
		for (const auto& extension : extensions) {
			std::cout << "\t" << extension.extensionName << "\n";
		}
	}

	// Force enable extensions needed by GLFW
	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();

	// Enable validation layers
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("Validation layers requested, but not available! Try installing the Lunar Vulkan SDK if you haven't already.");
	}

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}


	
	VkResult result = vkCreateInstance(&createInfo, nullptr, instance.replace()); assert(result == VK_SUCCESS);
}
void createDebugCallback(VkInstance instance, VDeleter<VkDebugReportCallbackEXT>& callback) {
	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

	if (createDebugReportCallbackEXT(instance, &createInfo, nullptr, callback.replace()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to set up debug callback.");
	}
}

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete() { return graphicsFamily >= 0 && presentFamily >= 0; }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
	QueueFamilyIndices indices;

	uint32 queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueCount <= 0) continue;

		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (presentSupport) {
			indices.presentFamily = i;
		}


		if (indices.isComplete()) {
			break;
		}
	}
	i++;

	return indices;
}


bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
	auto indices = findQueueFamilies(device, surface);
	
	auto checkDeviceExtensionSupport = [&]() -> bool {
		uint32 extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	};

	auto checkSwapChainSupport = [&]() -> bool {
		auto swapChainSupport = querySwapChainSupport(device, surface);
		return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	};

	return indices.isComplete() && checkDeviceExtensionSupport() && checkSwapChainSupport();
}
VkPhysicalDevice createPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
	VkPhysicalDevice pDevice = VK_NULL_HANDLE;

	uint32 deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) throw std::runtime_error("Failed to find GPU's with Vulkan support.");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (isDeviceSuitable(device, surface)) {
			pDevice = device;
			break;
		}
	}

	if (pDevice == VK_NULL_HANDLE) {
		std::runtime_error("Failed to find a suitable GPU!");
	}

	return pDevice;
}

void createLogicalDevice(VDeleter<VkDevice>& device, VkPhysicalDevice pDevice, VkSurfaceKHR surface, VkQueue& presentQueue, VkQueue& graphicsQueue) {
	QueueFamilyIndices indices = findQueueFamilies(pDevice, surface);
	
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int32> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };


	float queuePriority = 1.0f;
	for (auto queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = (uint32)queueCreateInfos.size();

	createInfo.pEnabledFeatures = &deviceFeatures;
	
	createInfo.enabledExtensionCount = deviceExtensions.size();
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}
	
	if (vkCreateDevice(pDevice, &createInfo, nullptr, device.replace()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create logical device");
	}

	vkGetDeviceQueue(device, indices.graphicsFamily, 0, &presentQueue);
	vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);

}

void createSurface(VkInstance instance, VDeleter<VkSurfaceKHR>& surface, GLFWwindow* window) {
	if (glfwCreateWindowSurface(instance, window, nullptr, surface.replace()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice pDevice, VkSurfaceKHR surface) {
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pDevice, surface, &details.capabilities);
	
	
	uint32 formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(pDevice, surface, &formatCount, details.formats.data());
	}

	
	uint32 presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(pDevice, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(pDevice, surface, &presentModeCount, details.presentModes.data());
	}


	return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	std::cout << "So sad." << "\n";
	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
		else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) bestMode = availablePresentMode;
	}

	return bestMode;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32>::max()) {
		return capabilities.currentExtent;
	}
	else {
		VkExtent2D actualExtent = { 1280, 720 };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

std::tuple<VkFormat, VkExtent2D> createSwapChain(VkInstance instance, VDeleter<VkSwapchainKHR>& swapChain, VkPhysicalDevice pDevice, VkSurfaceKHR surface, VkDevice device, std::vector<VkImage>& images) {
	auto swapChainSupport = querySwapChainSupport(pDevice, surface);

	auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	auto extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32 imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	auto indices = findQueueFamilies(pDevice, surface);
	uint32 queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, swapChain.replace()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	images.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());

	return { surfaceFormat.format, extent };
}

void createImageViews(std::vector<VDeleter<VkImageView>>& swapChainImageViews, const std::vector<VkImage>& swapChainImages, const VDeleter<VkDevice>& device, VkFormat format) {
	swapChainImageViews.resize(swapChainImages.size(), VDeleter<VkImageView>{ device, vkDestroyImageView });
	
	for (uint32 i = 0; i < swapChainImages.size(); i++) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &createInfo, nullptr, swapChainImageViews[i].replace()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create image views.");
		}
	}

}

int main() {
	Window window(1280, 720, "Praise kek");


	VDeleter<VkInstance> instance { vkDestroyInstance };
	VDeleter<VkDebugReportCallbackEXT> callback{ instance, destroyDebugReportCallbackEXT };
	VkPhysicalDevice pDevice = VK_NULL_HANDLE;
	VDeleter<VkDevice> device{ vkDestroyDevice };
	VkQueue graphicsQueue, presentQueue;
	VDeleter<VkSurfaceKHR> surface{ instance, vkDestroySurfaceKHR };
	VDeleter<VkSwapchainKHR> swapChain{ device, vkDestroySwapchainKHR };
	std::vector<VkImage> swapChainImages;
	VkFormat format; 
	VkExtent2D extent;
	std::vector<VDeleter<VkImageView>> swapChainImageViews;

	try {
		createVulkanInstance(instance);
		createDebugCallback(instance, callback);
		createSurface(instance, surface, window.nativeHandle);
		pDevice = createPhysicalDevice(instance, surface);
		createLogicalDevice(device, pDevice, surface, presentQueue, graphicsQueue);
		auto si = createSwapChain(instance, swapChain, pDevice, surface, device, swapChainImages);
		format = std::get<VkFormat>(si);
		extent = std::get<VkExtent2D>(si);
		createImageViews(swapChainImageViews, swapChainImages, device, format);
	}
	catch (std::runtime_error& error) {
		std::cout << error.what() << "\n";
		abort();
	}

	
	




	while (window.isOpen()) {
		glfwPollEvents();


	}

	return 0;
}