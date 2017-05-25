
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <Core/Vulkan/VDeleter.h>
#include <Core/Vulkan/VDebug.h>
#include <Core/Util/FileUtil.h>


#include <vulkan/vulkan.hpp>
#include <Window/Window.h>
#include <Core/Render/Vertex.h>
#include <Core/Render/Material.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <array>
#include <set>
#include <algorithm>
#include <chrono>

#include <Core/MeshLoaders/Ply.h>

#include <Core/Vulkan/DeviceLocalBuffer.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/image.h>

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

struct GeneralRenderUniforms {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

VKAPI_ATTR vk::Bool32 VKAPI_CALL vulkan_debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64 obj, size_t location, int32 code, const char* layerPrefix, const char* msg, void* userData) {

	std::cerr << "Validation layer: " << msg << std::endl;

	return VK_FALSE;
}

struct SwapChainSupportDetails {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};


SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice pDevice, vk::SurfaceKHR surface);

bool checkValidationLayerSupport() {
	auto availableLayers = vk::enumerateInstanceLayerProperties();

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

void createVulkanInstance(vk::Instance& instance) {
	vk::ApplicationInfo appInfo = {};
	appInfo.pApplicationName = "Praise kek";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	vk::InstanceCreateInfo createInfo = {};
	createInfo.pApplicationInfo = &appInfo;	

	// Query supported extensions and output them to the screen
	{
		auto extensions = vk::enumerateInstanceExtensionProperties();

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

	instance = vk::createInstance(createInfo);
}
void createDebugCallback(vk::Instance instance, vk::DebugReportCallbackEXT& debugCallback) {
	vk::DebugReportCallbackCreateInfoEXT createInfo;
	createInfo.flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning;
	createInfo.pfnCallback = vulkan_debugCallback;
	
	debugCallback = instance.createDebugReportCallbackEXT(createInfo);
}

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete() { return graphicsFamily >= 0 && presentFamily >= 0; }
};

QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
	QueueFamilyIndices indices;

	auto queueFamilies = device.getQueueFamilyProperties();

	for (int i = 0; i < queueFamilies.size(); i++) {
		auto& queueFamily = queueFamilies.at(i);

		if (queueFamily.queueCount <= 0) continue;

		if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = device.getSurfaceSupportKHR(i, surface);
		if (presentSupport) indices.presentFamily = i;

		if (indices.isComplete()) break;
	}

	return indices;
}

bool isDeviceSuitable(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
	auto indices = findQueueFamilies(device, surface);
	
	auto checkDeviceExtensionSupport = [&]() -> bool {
		auto availableExtensions = device.enumerateDeviceExtensionProperties();

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

	vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();

	return indices.isComplete() && checkDeviceExtensionSupport() && checkSwapChainSupport() 
		&& (supportedFeatures.samplerAnisotropy);
}

vk::PhysicalDevice createPhysicalDevice(vk::Instance instance, vk::SurfaceKHR surface) {
	vk::PhysicalDevice pDevice = VK_NULL_HANDLE;

	auto devices = instance.enumeratePhysicalDevices();

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

void createLogicalDevice(vk::Device& device, vk::PhysicalDevice pDevice, vk::SurfaceKHR surface, vk::Queue& presentQueue, vk::Queue& graphicsQueue) {
	QueueFamilyIndices indices = findQueueFamilies(pDevice, surface);
	
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
	createInfo.queueCreateInfoCount = (uint32)queueCreateInfos.size();

	createInfo.pEnabledFeatures = &deviceFeatures;
	
	createInfo.enabledExtensionCount = deviceExtensions.size();
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// Enable validation layers
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}
	
	device = pDevice.createDevice(createInfo);
	graphicsQueue = device.getQueue(indices.graphicsFamily, 0);
	presentQueue = device.getQueue(indices.presentFamily, 0);
}

void createSurface(vk::Instance instance, vk::SurfaceKHR& surface, GLFWwindow* window) {
	if (glfwCreateWindowSurface(static_cast<VkInstance>(instance), window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}
}

SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice pDevice, vk::SurfaceKHR surface) {
	SwapChainSupportDetails details;
	details.capabilities = pDevice.getSurfaceCapabilitiesKHR(surface);
	details.formats = pDevice.getSurfaceFormatsKHR(surface);
	details.presentModes = pDevice.getSurfacePresentModesKHR(surface);

	return details;
}
vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
	if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined) {
		return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	}

	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return availableFormat;
		}
	}

	return availableFormats[0];
}
vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> availablePresentModes) {
	vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;

	for (const auto& availablePresentMode : availablePresentModes) {
		if		(availablePresentMode == vk::PresentModeKHR::eMailbox) return availablePresentMode;
		else if (availablePresentMode == vk::PresentModeKHR::eImmediate) bestMode = availablePresentMode;
	}

	return bestMode;
}
vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32>::max()) {
		return capabilities.currentExtent;
	}
	else {
		vk::Extent2D actualExtent = { 1280, 720 };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

std::tuple<vk::Format, vk::Extent2D> createSwapChain(vk::Instance instance, vk::SwapchainKHR& swapChain, vk::PhysicalDevice pDevice, vk::SurfaceKHR surface, vk::Device device, std::vector<vk::Image>& images) {
	auto swapChainSupport = querySwapChainSupport(pDevice, surface);

	auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	auto extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32 imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR createInfo = {};
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

	auto indices = findQueueFamilies(pDevice, surface);
	uint32 queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode = presentMode;
	createInfo.clipped = true;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	swapChain = device.createSwapchainKHR(createInfo);

	images = device.getSwapchainImagesKHR(swapChain);
	return { surfaceFormat.format, extent };
}

void createImageViews(VulkanInstance& vulkan, std::vector<vk::ImageView>& swapChainImageViews, const std::vector<vk::Image>& swapChainImages, vk::Format format) {
	swapChainImageViews.resize(swapChainImages.size());
	
	for (uint32 i = 0; i < swapChainImages.size(); i++) {
		swapChainImageViews[i] = VkUtil::createImageView(vulkan, swapChainImages[i], format, vk::ImageAspectFlagBits::eColor);
	}

}

void createShaderModule(const std::vector<int8>& code, vk::Device device, vk::ShaderModule& shaderModule) {
	vk::ShaderModuleCreateInfo createInfo = {};
	createInfo.codeSize = code.size();

	std::vector<uint32> codeAligned(code.size() / sizeof(uint32_t) + 1);
	memcpy(codeAligned.data(), code.data(), code.size());
	createInfo.pCode = codeAligned.data();

	shaderModule = device.createShaderModule(createInfo);
}

void createRenderPass(vk::Device device, vk::RenderPass& renderPass, vk::Format format) {
	
	vk::AttachmentDescription colorAttachment = {};
	colorAttachment.format = format;
	colorAttachment.samples = vk::SampleCountFlagBits::e1;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
	
	vk::AttachmentReference colorAttachmentRef = { 0 };
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;


	vk::AttachmentDescription depthAttachment = {};
	depthAttachment.format = vk::Format::eD32Sfloat;
	depthAttachment.samples = vk::SampleCountFlagBits::e1;
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
	depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::AttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	depthAttachmentRef.attachment = 1;

	vk::SubpassDescription subpass = {};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;


	std::array<vk::AttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

	vk::RenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	

	vk::SubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;


	renderPass = device.createRenderPass(renderPassInfo);
}

void createPipeline(vk::Device device, vk::RenderPass renderPass, vk::DescriptorSetLayout& descriptorSetLayout, vk::PipelineLayout& pipelineLayout, vk::Pipeline& pipeline, vk::ShaderModule vertexShaderModule, vk::ShaderModule fragmentShaderModule, vk::Extent2D extent, Material& material) {
	vk::PipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertShaderStageInfo.module = vertexShaderModule;
	vertShaderStageInfo.pName = "main";

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragShaderStageInfo.module = fragmentShaderModule;
	fragShaderStageInfo.pName = "main";

	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.vertexBindingDescriptionCount = 1;

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vk::Viewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)extent.width;
	viewport.height = (float)extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vk::Rect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = extent;

	vk::PipelineViewportStateCreateInfo viewportState = {};
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	vk::PipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.depthClampEnable = false;
	rasterizer.rasterizerDiscardEnable = false;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizer.depthBiasEnable = false;

	vk::PipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sampleShadingEnable = false;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = false;

	vk::PipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.logicOpEnable = false;
	colorBlending.logicOp = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	
	// Create descriptor set layout
	{
		
		vk::DescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uboLayoutBinding.descriptorCount = 1;

		vk::DescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		std::array<vk::DescriptorSetLayoutBinding, 3> bindings = { uboLayoutBinding, samplerLayoutBinding };

		vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.bindingCount = 2;
		layoutInfo.pBindings = bindings.data();

		descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);
	}

	vk::PipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.depthTestEnable = true;
	depthStencil.depthWriteEnable = true;
	depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
	depthStencil.depthBoundsTestEnable = false;

	auto materialLayout = material.getDescriptorSetLayout(device);
	auto setLayouts = { descriptorSetLayout, materialLayout };

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.setLayoutCount = setLayouts.size();
	pipelineLayoutInfo.pSetLayouts = setLayouts.begin();
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

	// Create pipeline layout
	pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);
	
	// Create pipeline

	vk::GraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional


	pipeline = device.createGraphicsPipeline(nullptr, pipelineInfo);

}

int main() {
	Window window(1280, 720, "Praise kek");

	Mesh tableMesh = MeshLoaders::load_ply("meshes/table.ply");
	Material pbrMaterial;

	vk::Instance instance;
	vk::DebugReportCallbackEXT debugCallback;
	vk::PhysicalDevice pDevice;
	vk::Device device;
	vk::SurfaceKHR surface;
	vk::SwapchainKHR swapChain;

	vk::Queue graphicsQueue, presentQueue;
	std::vector<vk::Image> swapChainImages;
	std::vector<vk::ImageView> swapChainImageViews;

	vk::Format format;
	vk::Extent2D extent;

	vk::ShaderModule vertexShaderModule;
	vk::ShaderModule fragmentShaderModule;

	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline graphicsPipeline;
	std::vector<vk::Framebuffer> swapChainFramebuffers;
	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffers;

	vk::DescriptorSetLayout descriptorSetLayout;
	vk::DescriptorPool descriptorPool;
	vk::DescriptorSet descriptorSet;
	
	vk::Buffer uniformBuffer;
	vk::DeviceMemory uniformBufferMemory;

	vk::Semaphore imageAvailableSemaphore, renderFinishedSemaphore;

	vk::Image textureImage;
	vk::DeviceMemory textureImageMemory;
	vk::ImageView textureImageView;
	vk::Sampler textureImageSampler;

	vk::Image depthImage;
	vk::ImageView depthImageView;
	vk::DeviceMemory depthImageMemory;

	VulkanInstance vulkan;

	DeviceLocalBuffer vertexBuffer(vulkan, vk::BufferUsageFlagBits::eVertexBuffer);
	DeviceLocalBuffer indexBuffer(vulkan, vk::BufferUsageFlagBits::eIndexBuffer);

	auto cleanup_globals = [&]() {
		
		instance.destroy();
	};

	try {
		createVulkanInstance(instance);
		createDebugCallback(instance, debugCallback);
		vulkan.instance = instance;

		createSurface(instance, surface, window.nativeHandle);
		vulkan.surface = surface;

		pDevice = createPhysicalDevice(instance, surface);
		vulkan.physicalDevice = pDevice;

		createLogicalDevice(device, pDevice, surface, presentQueue, graphicsQueue);
		vulkan.device = device;

		auto si = createSwapChain(instance, swapChain, pDevice, surface, device, swapChainImages);
		format = std::get<vk::Format>(si);
		extent = std::get<vk::Extent2D>(si);

		vulkan.graphicsQueue = graphicsQueue;
		vulkan.presentQueue = presentQueue;
		vulkan.utilityPool = commandPool;
		

		createImageViews(vulkan, swapChainImageViews, swapChainImages, format);
	
		auto vertShaderCode = FUtil::file_read_binary("shaders/vert.spv");
		auto fragShaderCode = FUtil::file_read_binary("shaders/frag.spv");

		createShaderModule(vertShaderCode, device, vertexShaderModule);
		createShaderModule(fragShaderCode, device, fragmentShaderModule);

		createRenderPass(device, renderPass, format);
		createPipeline(device, renderPass, descriptorSetLayout, pipelineLayout, graphicsPipeline, vertexShaderModule, fragmentShaderModule, extent, pbrMaterial);
		



		// Create command pool
		{
			auto indices = findQueueFamilies(pDevice, surface);

			vk::CommandPoolCreateInfo poolInfo = {};
			poolInfo.queueFamilyIndex = indices.graphicsFamily;

			commandPool = device.createCommandPool(poolInfo);

			// Use the graphics family as our utility pool since it supports what we need
			vulkan.createUtilityPool(indices.graphicsFamily);
		}
		
		

		// Create depth buffer
		{
			vk::Format depthFormat = vk::Format::eD32Sfloat;
			VkUtil::createImage(vulkan, depthImage, depthImageMemory, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
			depthImageView = VkUtil::createImageView(vulkan, depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
			VkUtil::transitionImageLayout(vulkan, depthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
		}

		// Create framebuffers
		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vk::ImageView attachments[] = {
				swapChainImageViews[i],
				depthImageView
			};

			vk::FramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 2;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = extent.width;
			framebufferInfo.height = extent.height;
			framebufferInfo.layers = 1;

			swapChainFramebuffers[i] = device.createFramebuffer(framebufferInfo);
		}

		// Create vertex buffer		
		vertexBuffer.fill(tableMesh.vertices.data(), tableMesh.vertices.size() * sizeof(Vertex));
		indexBuffer.fill(tableMesh.indices.data(), tableMesh.indices.size() * sizeof(uint32));

		// Create uniform buffers
		{
			vk::DeviceSize bufferSize = sizeof(GeneralRenderUniforms);

			vk::BufferCreateInfo bufferInfo = {};
			bufferInfo.size = bufferSize;
			bufferInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
			bufferInfo.sharingMode = vk::SharingMode::eExclusive;

			uniformBuffer = device.createBuffer(bufferInfo);

			VkMemoryRequirements memoryRequirements = device.getBufferMemoryRequirements(uniformBuffer);

			auto findMemoryType = [&](uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
				vk::PhysicalDeviceMemoryProperties memProperties = pDevice.getMemoryProperties();

				for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
					if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
						return i;
					}
				}

				throw std::runtime_error("failed to find suitable memory type!");
			};


			vk::MemoryAllocateInfo allocInfo = {};
			allocInfo.allocationSize = memoryRequirements.size;
			allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

			uniformBufferMemory = device.allocateMemory(allocInfo);
			device.bindBufferMemory(uniformBuffer, uniformBufferMemory, 0);
		}

		// Load texture
		{
			int texWidth, texHeight, texChannels;
			uint8* pixels = stbi_load("textures/test.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			vk::DeviceSize imageSize = texWidth * texHeight * 4;

			if (!pixels) throw std::runtime_error("Failed to load texture.");
			
			vk::Buffer stagingBuffer;
			vk::DeviceMemory stagingBufferMemory;

			VkUtil::createBuffer(vulkan, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

			void* data = device.mapMemory(stagingBufferMemory, 0, imageSize);
			memcpy(data, pixels, imageSize);
			device.unmapMemory(stagingBufferMemory);

			stbi_image_free(pixels);

			vk::Extent2D imgExtent = { (uint32)texWidth, (uint32)texHeight };

			VkUtil::createImage(vulkan, textureImage, textureImageMemory, imgExtent, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
			VkUtil::transitionImageLayout(vulkan, textureImage, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eTransferDstOptimal);
			VkUtil::copyBufferToImage(vulkan, stagingBuffer, textureImage, imgExtent);
			VkUtil::transitionImageLayout(vulkan, textureImage, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

			device.destroyBuffer(stagingBuffer);
			device.freeMemory(stagingBufferMemory);

			// Create image view
			textureImageView = VkUtil::createImageView(vulkan, textureImage, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);
			
			// Create texture sampler
			vk::SamplerCreateInfo samplerInfo = {};
			samplerInfo.magFilter = vk::Filter::eLinear;
			samplerInfo.minFilter = vk::Filter::eLinear;
			samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
			samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
			samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
			samplerInfo.anisotropyEnable = true;
			samplerInfo.maxAnisotropy = 16.f;
			samplerInfo.unnormalizedCoordinates = false;
			samplerInfo.compareEnable = false;
			samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 0.0f;
			
			textureImageSampler = device.createSampler(samplerInfo);
		}

		// Create descriptor pool
		{
			std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
			poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
			poolSizes[0].descriptorCount = 1;
			poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
			poolSizes[1].descriptorCount = 1;

			vk::DescriptorPoolCreateInfo poolInfo = {};
			poolInfo.poolSizeCount = poolSizes.size();
			poolInfo.pPoolSizes = poolSizes.data();
			poolInfo.maxSets = 1;

			descriptorPool = device.createDescriptorPool(poolInfo);	
		}

		// Create descriptor set
		{
			vk::DescriptorSetLayout layouts[] = { descriptorSetLayout };
			vk::DescriptorSetAllocateInfo allocInfo = {};
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = layouts;

			descriptorSet = device.allocateDescriptorSets(allocInfo)[0];

			vk::DescriptorBufferInfo bufferInfo = {};
			bufferInfo.buffer = uniformBuffer;
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(GeneralRenderUniforms);

			vk::DescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			imageInfo.imageView = textureImageView;
			imageInfo.sampler = textureImageSampler;
			
			std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};

			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo;

			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			device.updateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
		}


		// Create command buffers
		{
			commandBuffers.resize(swapChainFramebuffers.size());
			vk::CommandBufferAllocateInfo allocInfo = {};
			allocInfo.commandPool = commandPool;
			allocInfo.level = vk::CommandBufferLevel::ePrimary;
			allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

			commandBuffers = device.allocateCommandBuffers(allocInfo);
		}


		// record commands
		{
			for (size_t i = 0; i < commandBuffers.size(); i++) {
				auto& commandBuffer = commandBuffers[i];

				vk::CommandBufferBeginInfo beginInfo = {};
				beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
				beginInfo.pInheritanceInfo = nullptr; // Optional

				commandBuffer.begin(beginInfo);

				vk::RenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.renderPass = renderPass;
				renderPassInfo.framebuffer = swapChainFramebuffers[i];
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = extent;

				std::array<vk::ClearValue, 2> clearColors = {};
				clearColors[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.05f, 0.05f, 0.05f, 1.0f });
				clearColors[1].depthStencil = { 1.0, 0 };

				renderPassInfo.clearValueCount = 2;
				renderPassInfo.pClearValues = clearColors.data();

				commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
				commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

				vk::DeviceSize offsets[] = { 0 };
				
				commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer.buffer, offsets);
				commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
				commandBuffer.drawIndexed(static_cast<uint32>(tableMesh.indices.size()), 1, 0, 0, 0);
				commandBuffer.endRenderPass();
				commandBuffer.end();
			}

			// Create semaphores
			vk::SemaphoreCreateInfo semaphoreInfo = {};
			imageAvailableSemaphore = device.createSemaphore(semaphoreInfo);
			renderFinishedSemaphore = device.createSemaphore(semaphoreInfo);
		
		}
	}
	catch (std::runtime_error& error) {
		std::cout << error.what() << "\n";
		abort();
	}

	
	

	auto projection = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 100.0f);
	auto view = glm::lookAt(glm::vec3(5.0f, 5.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));;
	projection[1][1] *= -1;
	GeneralRenderUniforms ubo = {};
	
	ubo.view = view;
	ubo.projection = projection;
	while (window.isOpen()) {
		glfwPollEvents();		

		// Update uniforms
		{
			static auto startTime = std::chrono::high_resolution_clock().now();
			static auto i = 0;
			i++;

			auto currentTime = std::chrono::high_resolution_clock().now();
			float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.f;

			
			ubo.model = glm::rotate(glm::mat4(), time * glm::radians(90.f), glm::vec3(0.0, 0.0, 1.0));

			void* data;
			data = device.mapMemory(uniformBufferMemory, 0, sizeof(ubo));
			memcpy(data, &ubo, sizeof(ubo));
			device.unmapMemory(uniformBufferMemory);

			// 10 seconds passed
			if (time > 10) {
				std::cout << "Rendered " << i << " frames in 10 seconds.\nFPS: " << i / 10 << "\n";
				std::cin.get();
			}
		}

		// render
		uint32_t imageIndex = device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, nullptr).value;

		vk::SubmitInfo submitInfo = {};

		vk::Semaphore waitSemaphores[] = { imageAvailableSemaphore };
		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		vk::Semaphore signalSemaphores[] = { renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		graphicsQueue.submit(1, &submitInfo, nullptr);

		vk::PresentInfoKHR presentInfo = {};
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		vk::SwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Optional
		presentQueue.presentKHR(presentInfo);
	}

	return 0;
}