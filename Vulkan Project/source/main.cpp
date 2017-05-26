
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

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
#include <Core/Vulkan/HostCoherentBuffer.h>

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

struct PointLight {
	glm::vec3 position;
	float intensity = 1;
};

struct Lights {
	PointLight pointLights[16];
	uint32 pointLightCount = 0;
};

struct SwapChainSupportDetails {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};

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

std::tuple<vk::Format, vk::Extent2D> createSwapChain(VulkanInstance& vulkan, vk::Instance instance, vk::SwapchainKHR& swapChain, vk::PhysicalDevice pDevice, vk::SurfaceKHR surface, vk::Device device, std::vector<vk::Image>& images) {
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

	auto indices = vulkan.findQueueFamilyIndices(vulkan.physicalDevice);
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

		vk::DescriptorSetLayoutBinding lightLayoutBinding = {};
		lightLayoutBinding.binding = 10;
		lightLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		lightLayoutBinding.descriptorCount = 1;
		lightLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		std::array<vk::DescriptorSetLayoutBinding, 3> bindings = { uboLayoutBinding, samplerLayoutBinding, lightLayoutBinding };

		vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.bindingCount = bindings.size();
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

	Lights lights;
	lights.pointLights[0].position = glm::vec3(1, 2, 0);
	lights.pointLights[1].position = glm::vec3(-4, -2, 1);

	lights.pointLightCount = 1;

	VulkanInstance vulkan;
	vulkan.createVulkanInstance();
	vulkan.createSurface(window.nativeHandle);
	vulkan.createPhysicalDevice();
	vulkan.createLogicalDevice();
	vulkan.createUtilityPool();

	vk::SwapchainKHR swapChain;

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

	vk::Semaphore imageAvailableSemaphore, renderFinishedSemaphore;

	vk::Image textureImage;
	vk::DeviceMemory textureImageMemory;
	vk::ImageView textureImageView;
	vk::Sampler textureImageSampler;

	vk::Image depthImage;
	vk::ImageView depthImageView;
	vk::DeviceMemory depthImageMemory;


	DeviceLocalBuffer vertexBuffer(vulkan, vk::BufferUsageFlagBits::eVertexBuffer);
	DeviceLocalBuffer indexBuffer(vulkan, vk::BufferUsageFlagBits::eIndexBuffer);
	HostCoherentBuffer uniformBuffer(vulkan, vk::BufferUsageFlagBits::eUniformBuffer);
	HostCoherentBuffer lightUniformBuffer(vulkan, vk::BufferUsageFlagBits::eUniformBuffer);

	/* Deferred renderer! */
	vk::ShaderModule geometryVertexShader;
	vk::ShaderModule geometryFragmentShader;

	vk::RenderPass geometryPass;
	vk::PipelineLayout geometryPipelineLayout;
	vk::Pipeline geometryPipeline;
	vk::DescriptorSetLayout geometryPassDescriptorSetLayout;
	vk::Framebuffer geometryFramebuffer;

	vk::Image gPositionBuffer;
	vk::ImageView gPositionView;
	vk::DeviceMemory gPositionMemory;

	vk::Image gNormalBuffer;
	vk::ImageView gNormalView;
	vk::DeviceMemory gNormalMemory;

	vk::CommandBuffer geometryPassCommandBuffer;

	try {
		auto si = createSwapChain(vulkan, vulkan.instance, swapChain, vulkan.physicalDevice, vulkan.surface, vulkan.device, swapChainImages);
		format = std::get<vk::Format>(si);
		extent = std::get<vk::Extent2D>(si);

		createImageViews(vulkan, swapChainImageViews, swapChainImages, format);
	
		auto vertShaderCode = FUtil::file_read_binary("shaders/tri.vert.spv");
		auto fragShaderCode = FUtil::file_read_binary("shaders/tri.frag.spv");

		createShaderModule(vertShaderCode, vulkan.device, vertexShaderModule);
		createShaderModule(fragShaderCode, vulkan.device, fragmentShaderModule);

		/* Create deferred render pipeline */
		createShaderModule(FUtil::file_read_binary("shaders/geom.vert.spv"), vulkan.device, geometryVertexShader);
		createShaderModule(FUtil::file_read_binary("shaders/geom.frag.spv"), vulkan.device, geometryFragmentShader);

		/* Create render pass */
		{
			vk::AttachmentDescription attachmentBlueprint = { };
			attachmentBlueprint.samples = vk::SampleCountFlagBits::e1;
			attachmentBlueprint.loadOp = vk::AttachmentLoadOp::eClear;
			attachmentBlueprint.storeOp = vk::AttachmentStoreOp::eStore;
			attachmentBlueprint.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
			attachmentBlueprint.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
			attachmentBlueprint.initialLayout = vk::ImageLayout::eUndefined;
			attachmentBlueprint.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::AttachmentDescription positionAttachment = attachmentBlueprint;
			positionAttachment.format = vk::Format::eR16G16B16A16Sfloat;

			vk::AttachmentDescription normalAttachment = attachmentBlueprint;
			normalAttachment.format = vk::Format::eR16G16B16A16Sfloat;

			vk::AttachmentReference positionAttachmentRef = { };
			positionAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
			positionAttachmentRef.attachment = 0;

			vk::AttachmentReference normalAttachmentRef = {};
			normalAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
			normalAttachmentRef.attachment = 1;


			vk::AttachmentDescription depthAttachment = attachmentBlueprint;
			depthAttachment.format = vk::Format::eD32Sfloat;
			depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;			
			vk::AttachmentReference depthAttachmentRef(2, vk::ImageLayout::eDepthStencilAttachmentOptimal);

			auto colorAttachments = { positionAttachmentRef, normalAttachmentRef };

			vk::SubpassDescription subpass = {};
			subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
			subpass.colorAttachmentCount = 2;
			subpass.pColorAttachments = colorAttachments.begin();
			subpass.pDepthStencilAttachment = &depthAttachmentRef;

			auto attachments = { positionAttachment, normalAttachment, depthAttachment };

			vk::RenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.attachmentCount = attachments.size();
			renderPassInfo.pAttachments = attachments.begin();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;

			vk::SubpassDependency dependency = {};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;

			geometryPass = vulkan.device.createRenderPass(renderPassInfo);
		}

		{
			vk::PipelineShaderStageCreateInfo vertexShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex, geometryVertexShader, "main");
			vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment, geometryFragmentShader, "main");

			auto shaderStages = { vertexShaderStageInfo, fragmentShaderStageInfo };

			vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};			
			auto bindingDescription = Vertex::getBindingDescription();
			auto attributeDescriptions = Vertex::getAttributeDescriptions();
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

			vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, false);
			vk::Viewport viewport = {};
			viewport.width = (float)extent.width;
			viewport.height = (float)extent.height;
			viewport.maxDepth = 1.0f;

			vk::Rect2D scissor = {};
			scissor.extent = extent;

			vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);
			vk::PipelineRasterizationStateCreateInfo rasterizer = { };
			rasterizer.polygonMode = vk::PolygonMode::eFill;
			rasterizer.lineWidth = 1.0f;
			rasterizer.cullMode = vk::CullModeFlagBits::eNone;
			rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
			
			vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB;
			colorBlendAttachment.blendEnable = false;

			auto colorBlendAttachments = { colorBlendAttachment, colorBlendAttachment };

			vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, false);
			vk::PipelineColorBlendStateCreateInfo colorBlending({}, false, {}, 2, colorBlendAttachments.begin());
		
			// Create descriptor set layout
			{
				vk::DescriptorSetLayoutBinding mvpLayoutBinding = {};
				mvpLayoutBinding.binding = 0;
				mvpLayoutBinding.descriptorCount = 1;
				mvpLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
				mvpLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

				auto bindings = { mvpLayoutBinding };

				vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
				layoutInfo.bindingCount = bindings.size();
				layoutInfo.pBindings = bindings.begin();

				geometryPassDescriptorSetLayout = vulkan.device.createDescriptorSetLayout(layoutInfo);
			}
			
			vk::PipelineDepthStencilStateCreateInfo depthStencil = {};
			depthStencil.depthTestEnable = true;
			depthStencil.depthWriteEnable = true;
			depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;

			
			auto setLayouts = { geometryPassDescriptorSetLayout };

			vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {};
			pipelineLayoutInfo.setLayoutCount = setLayouts.size();
			pipelineLayoutInfo.pSetLayouts = setLayouts.begin();
			
			geometryPipelineLayout = vulkan.device.createPipelineLayout(pipelineLayoutInfo);


			vk::GraphicsPipelineCreateInfo pipelineInfo = {};
			pipelineInfo.stageCount = shaderStages.size();
			pipelineInfo.pStages = shaderStages.begin();
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pDepthStencilState = &depthStencil;
			pipelineInfo.pColorBlendState = &colorBlending;
			pipelineInfo.layout = geometryPipelineLayout;
			pipelineInfo.renderPass = geometryPass;

			geometryPipeline = vulkan.device.createGraphicsPipeline(nullptr, pipelineInfo);
		}


		// Create depth buffer
		{
			vk::Format depthFormat = vk::Format::eD32Sfloat;
			VkUtil::createImage(vulkan, depthImage, depthImageMemory, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
			depthImageView = VkUtil::createImageView(vulkan, depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
			VkUtil::transitionImageLayout(vulkan, depthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
		}

		// Create g buffer
		{
			vk::Format gFormat = vk::Format::eR16G16B16A16Sfloat;
			VkUtil::createImage(vulkan, gPositionBuffer, gPositionMemory, extent, gFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
			gPositionView = VkUtil::createImageView(vulkan, gPositionBuffer, gFormat, vk::ImageAspectFlagBits::eColor);
			VkUtil::transitionImageLayout(vulkan, gPositionBuffer, gFormat, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eColorAttachmentOptimal);
	

			VkUtil::createImage(vulkan, gNormalBuffer, gPositionMemory, extent, gFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
			gNormalView = VkUtil::createImageView(vulkan, gNormalBuffer, gFormat, vk::ImageAspectFlagBits::eColor);
			VkUtil::transitionImageLayout(vulkan, gNormalBuffer, gFormat, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eColorAttachmentOptimal);

		}
		
		auto attachments = {
			gPositionView,
			gNormalView,
			depthImageView
		};

		vk::FramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.renderPass = geometryPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.begin();
		framebufferInfo.width = extent.width;
		framebufferInfo.height = extent.height;
		framebufferInfo.layers = 1;
		geometryFramebuffer = vulkan.device.createFramebuffer(framebufferInfo);

		createRenderPass(vulkan.device, renderPass, format);
		createPipeline(vulkan.device, renderPass, descriptorSetLayout, pipelineLayout, graphicsPipeline, vertexShaderModule, fragmentShaderModule, extent, pbrMaterial);
		

		uniformBuffer.resize(sizeof(GeneralRenderUniforms));
		lightUniformBuffer.resize(sizeof(Lights));

		// Create command pool
		{
			auto indices = vulkan.findQueueFamilyIndices(vulkan.physicalDevice);

			vk::CommandPoolCreateInfo poolInfo = {};
			poolInfo.queueFamilyIndex = indices.graphicsFamily;

			commandPool = vulkan.device.createCommandPool(poolInfo);
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

			swapChainFramebuffers[i] = vulkan.device.createFramebuffer(framebufferInfo);
		}

		// Create vertex buffer		
		vertexBuffer.fill(tableMesh.vertices.data(), tableMesh.vertices.size() * sizeof(Vertex));
		indexBuffer.fill(tableMesh.indices.data(), tableMesh.indices.size() * sizeof(uint32));

		// Load texture
		{
			int texWidth, texHeight, texChannels;
			uint8* pixels = stbi_load("textures/test.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			vk::DeviceSize imageSize = texWidth * texHeight * 4;

			if (!pixels) throw std::runtime_error("Failed to load texture.");
			
			vk::Buffer stagingBuffer;
			vk::DeviceMemory stagingBufferMemory;

			VkUtil::createBuffer(vulkan, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

			void* data = vulkan.device.mapMemory(stagingBufferMemory, 0, imageSize);
			memcpy(data, pixels, imageSize);
			vulkan.device.unmapMemory(stagingBufferMemory);

			stbi_image_free(pixels);

			vk::Extent2D imgExtent = { (uint32)texWidth, (uint32)texHeight };

			VkUtil::createImage(vulkan, textureImage, textureImageMemory, imgExtent, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
			VkUtil::transitionImageLayout(vulkan, textureImage, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eTransferDstOptimal);
			VkUtil::copyBufferToImage(vulkan, stagingBuffer, textureImage, imgExtent);
			VkUtil::transitionImageLayout(vulkan, textureImage, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

			vulkan.device.destroyBuffer(stagingBuffer);
			vulkan.device.freeMemory(stagingBufferMemory);

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
			
			textureImageSampler = vulkan.device.createSampler(samplerInfo);
		}

		// Create descriptor pool
		{
			std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
			poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
			poolSizes[0].descriptorCount = 2;
			poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
			poolSizes[1].descriptorCount = 1;

			vk::DescriptorPoolCreateInfo poolInfo = {};
			poolInfo.poolSizeCount = poolSizes.size();
			poolInfo.pPoolSizes = poolSizes.data();
			poolInfo.maxSets = 1;

			descriptorPool = vulkan.device.createDescriptorPool(poolInfo);	
		}

		// Create descriptor set
		{
			vk::DescriptorSetLayout layouts[] = { geometryPassDescriptorSetLayout };
			vk::DescriptorSetAllocateInfo allocInfo = {};
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = layouts;

			descriptorSet = vulkan.device.allocateDescriptorSets(allocInfo)[0];

			vk::DescriptorBufferInfo bufferInfo = {};
			bufferInfo.buffer = uniformBuffer.buffer;
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(GeneralRenderUniforms);

			vk::DescriptorBufferInfo lBufferInfo = {};
			lBufferInfo.buffer = lightUniformBuffer.buffer;
			lBufferInfo.offset = 0;
			lBufferInfo.range = sizeof(Lights);

			vk::DescriptorImageInfo imageInfo = {};
			imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			imageInfo.imageView = textureImageView;
			imageInfo.sampler = textureImageSampler;
			
			std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};

			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo;

			//descriptorWrites[1].dstSet = descriptorSet;
			//descriptorWrites[1].dstBinding = 1;
			//descriptorWrites[1].dstArrayElement = 0;
			//descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
			//descriptorWrites[1].descriptorCount = 1;
			//descriptorWrites[1].pImageInfo = &imageInfo;

			//descriptorWrites[2].dstSet = descriptorSet;
			//descriptorWrites[2].dstBinding = 10;
			//descriptorWrites[2].dstArrayElement = 0;
			//descriptorWrites[2].descriptorType = vk::DescriptorType::eUniformBuffer;
			//descriptorWrites[2].descriptorCount = 1;
			//descriptorWrites[2].pBufferInfo = &lBufferInfo;

			vulkan.device.updateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
		}


		// Create command buffers
		{
			commandBuffers.resize(swapChainFramebuffers.size());
			vk::CommandBufferAllocateInfo allocInfo = {};
			allocInfo.commandPool = commandPool;
			allocInfo.level = vk::CommandBufferLevel::ePrimary;
			allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

			commandBuffers = vulkan.device.allocateCommandBuffers(allocInfo);
		}

		{
			vk::CommandBufferAllocateInfo allocInfo = {};
			allocInfo.commandPool = commandPool;
			allocInfo.level = vk::CommandBufferLevel::ePrimary;
			allocInfo.commandBufferCount = 1;

			geometryPassCommandBuffer = vulkan.device.allocateCommandBuffers(allocInfo)[0];

		}

		// record commands
		{
			// Geometry pass
			auto& commandBuffer = geometryPassCommandBuffer;

			vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			commandBuffer.begin(beginInfo);

			std::array<vk::ClearValue, 3> clearColors = {};
			clearColors[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f });
			clearColors[1].color = vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f });
			clearColors[2].depthStencil = { 1.0, 0 };

			vk::RenderPassBeginInfo renderPassInfo(geometryPass, geometryFramebuffer, vk::Rect2D({ 0, 0, }, extent), 3, clearColors.data());
			commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, geometryPipeline);
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, geometryPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			
			vk::DeviceSize offsets[] = { 0 };

			commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer.buffer, offsets);
			commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
			commandBuffer.drawIndexed(tableMesh.indices.size(), 1, 0, 0, 0);
			commandBuffer.endRenderPass();
			commandBuffer.end();

			for (size_t i = 0; i < commandBuffers.size(); i++) {
				auto& commandBuffer = commandBuffers[i];

				vk::CommandBufferBeginInfo beginInfo = {};
				beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;
				beginInfo.pInheritanceInfo = nullptr; // Optional

				commandBuffer.begin(beginInfo);

				vk::RenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.renderPass = geometryPass;
				renderPassInfo.framebuffer = geometryFramebuffer;
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = extent;

				std::array<vk::ClearValue, 3> clearColors = {};
				clearColors[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.f, 0.f, 0.f, 0.f });
				clearColors[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.f, 0.f, 0.f, 0.f });
				clearColors[1].depthStencil = { 1.0, 0 };

				renderPassInfo.clearValueCount = 3;
				renderPassInfo.pClearValues = clearColors.data();

				commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, geometryPipeline);
				commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, geometryPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

				vk::DeviceSize offsets[] = { 0 };
				
				commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer.buffer, offsets);
				commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
				commandBuffer.drawIndexed(static_cast<uint32>(tableMesh.indices.size()), 1, 0, 0, 0);
				commandBuffer.endRenderPass();
				commandBuffer.end();
			}

			// Create semaphores
			vk::SemaphoreCreateInfo semaphoreInfo = {};
			imageAvailableSemaphore = vulkan.device.createSemaphore(semaphoreInfo);
			renderFinishedSemaphore = vulkan.device.createSemaphore(semaphoreInfo);
		
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

			uniformBuffer.fill(&ubo, sizeof(ubo));

			lightUniformBuffer.fill(&lights, sizeof(Lights));

			// 10 seconds passed
			if (time > 10) {
				std::cout << "Rendered " << i << " frames in 10 seconds.\nFPS: " << i / 10 << "\n";
				std::cin.get();
			}
		}

		// render
		uint32_t imageIndex = vulkan.device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, nullptr).value;

		vk::SubmitInfo submitInfo = {};

		vk::Semaphore waitSemaphores[] = { imageAvailableSemaphore };
		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &geometryPassCommandBuffer;

		vk::Semaphore signalSemaphores[] = { renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vulkan.graphicsQueue.submit(1, &submitInfo, nullptr);

		vk::PresentInfoKHR presentInfo = {};
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		vk::SwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Optional
		vulkan.presentQueue.presentKHR(presentInfo);
	}

	vulkan.device.waitIdle();
	return 0;
}