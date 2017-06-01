
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <Core/Vulkan/VDebug.h>
#include <Core/Util/FileUtil.h>


#include <vulkan/vulkan.hpp>
#include <Window/Window.h>
#include <Core/Render/Camera.h>
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

int main() {
	Window window(1280, 720, "Praise kek");
	Camera camera(Transform(glm::vec3(0, 5, 3)), glm::perspective(glm::radians(75.0f), 1280.f / 720.f, 0.1f, 100.0f));

	Mesh tableMesh = MeshLoaders::load_ply("meshes/table.ply");
	Material pbrMaterial;

	Lights lights;
	lights.pointLights[0].position = glm::vec3(-2, 5, 0);
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

	std::vector<vk::Framebuffer> swapChainFramebuffers;
	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffers;

	vk::DescriptorPool descriptorPool;

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
	vk::Framebuffer geometryFramebuffer;

	vk::Image gPositionBuffer;
	vk::ImageView gPositionView;
	vk::DeviceMemory gPositionMemory;

	vk::Image gNormalBuffer;
	vk::ImageView gNormalView;
	vk::DeviceMemory gNormalMemory;

	vk::CommandBuffer geometryPassCommandBuffer;

	/* Normal renderer */
	vk::ShaderModule lightingVertexShader;
	vk::ShaderModule lightingFragmentShader;

	vk::RenderPass lightingPass;
	vk::Pipeline lightingPipeline;
	vk::PipelineLayout lightingPipelineLayout;

	DeviceLocalBuffer screenQuadBuffer(vulkan, vk::BufferUsageFlagBits::eVertexBuffer);
	Vertex screenQuad[] = {
		{ glm::vec3(-1, -1, 0) }, { glm::vec3(1, -1, 0) },
		{ glm::vec3(-1, 1, 0) }, { glm::vec3(1, 1, 0) }
	};
	screenQuadBuffer.fill(screenQuad, sizeof(Vertex) * 4);

	/*
		Refactor
	*/
	vk::DescriptorSetLayout gBufferLayout;
	vk::DescriptorSetLayout mvpBufferLayout;
	vk::DescriptorSetLayout lightBufferLayout;

	vk::DescriptorSet gBufferSet, mvpBufferSet, lightBufferSet;
	vk::Sampler gBufferSampler;

	// Create descriptor set layouts 
	{
		{
			vk::DescriptorSetLayoutBinding mvpLayoutBinding = {};
			mvpLayoutBinding.binding = 0;
			mvpLayoutBinding.descriptorCount = 1;
			mvpLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
			mvpLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &mvpLayoutBinding;

			mvpBufferLayout = vulkan.device.createDescriptorSetLayout(layoutInfo);
		}

		{
			vk::DescriptorSetLayoutBinding positionBuffer = {};
			positionBuffer.binding = 0;
			positionBuffer.descriptorCount = 1;
			positionBuffer.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			positionBuffer.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutBinding normalBuffer = {};
			normalBuffer.binding = 1;
			normalBuffer.descriptorCount = 1;
			normalBuffer.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			normalBuffer.stageFlags = vk::ShaderStageFlagBits::eFragment;

			auto gBufferBindings = { positionBuffer, normalBuffer };
			vk::DescriptorSetLayoutCreateInfo layoutInfo({}, gBufferBindings.size(), gBufferBindings.begin());
			gBufferLayout = vulkan.device.createDescriptorSetLayout(layoutInfo);
		}

		{
			vk::DescriptorSetLayoutBinding lightBuffer = {};
			lightBuffer.binding = 0;
			lightBuffer.descriptorCount = 1;
			lightBuffer.descriptorType = vk::DescriptorType::eUniformBuffer;
			lightBuffer.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &lightBuffer);
			lightBufferLayout = vulkan.device.createDescriptorSetLayout(layoutInfo);
		}
	}



	try {
		auto si = createSwapChain(vulkan, vulkan.instance, swapChain, vulkan.physicalDevice, vulkan.surface, vulkan.device, swapChainImages);
		format = std::get<vk::Format>(si);
		extent = std::get<vk::Extent2D>(si);

		createImageViews(vulkan, swapChainImageViews, swapChainImages, format);
	
		auto vertShaderCode = FUtil::file_read_binary("shaders/tri.vert.spv");
		auto fragShaderCode = FUtil::file_read_binary("shaders/tri.frag.spv");

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
			
			vk::PipelineDepthStencilStateCreateInfo depthStencil = {};
			depthStencil.depthTestEnable = true;
			depthStencil.depthWriteEnable = true;
			depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
			
			auto setLayouts = { mvpBufferLayout };

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

		
		/* Create lighting pass render pipeline */
		createShaderModule(FUtil::file_read_binary("shaders/tri.vert.spv"), vulkan.device, lightingVertexShader);
		createShaderModule(FUtil::file_read_binary("shaders/tri.frag.spv"), vulkan.device, lightingFragmentShader);

		/* Create render pass */
		{
			vk::AttachmentDescription colorAttachment;
			colorAttachment.format = format;
			colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
			colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
			colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
			colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
			colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
			colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
			colorAttachment.samples = vk::SampleCountFlagBits::e1;
			
			vk::AttachmentReference colorAttachmentRef;
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
			
			vk::SubpassDescription subpass;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
			
			vk::RenderPassCreateInfo renderPassInfo;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &colorAttachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;

			lightingPass = vulkan.device.createRenderPass(renderPassInfo);
		}

		/* Create lighting pipeline */
		{
			vk::PipelineShaderStageCreateInfo vertexShaderInfo;
			vertexShaderInfo.module = lightingVertexShader;
			vertexShaderInfo.pName = "main";
			vertexShaderInfo.stage = vk::ShaderStageFlagBits::eVertex;

			vk::PipelineShaderStageCreateInfo fragmentShaderInfo;
			fragmentShaderInfo.module = lightingFragmentShader;
			fragmentShaderInfo.pName = "main";
			fragmentShaderInfo.stage = vk::ShaderStageFlagBits::eFragment;

			auto shaders = { vertexShaderInfo, fragmentShaderInfo };

			auto& binding = Vertex::getBindingDescription();
			auto& attributes = Vertex::getAttributeDescriptions();
			vk::PipelineVertexInputStateCreateInfo vertexInfo;
			vertexInfo.vertexBindingDescriptionCount = 1;
			vertexInfo.pVertexBindingDescriptions = &binding;
			vertexInfo.vertexAttributeDescriptionCount = attributes.size();
			vertexInfo.pVertexAttributeDescriptions = attributes.data();
		

			vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
			inputAssembly.topology = vk::PrimitiveTopology::eTriangleStrip;
			inputAssembly.primitiveRestartEnable = false;
			
			vk::Viewport viewport;
			viewport.width = extent.width;
			viewport.height = extent.height;
			viewport.maxDepth = 1.0f;
			viewport.x = 0;
			viewport.y = 0;
			
			vk::Rect2D scissor;
			scissor.extent = extent;
			scissor.offset = { 0, 0, };

			vk::PipelineViewportStateCreateInfo viewportState;
			viewportState.viewportCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.scissorCount = 1;
			viewportState.pScissors = &scissor;

			vk::PipelineRasterizationStateCreateInfo rasterizer;
			rasterizer.cullMode = vk::CullModeFlagBits::eNone;
			rasterizer.lineWidth = 1.0f;
			rasterizer.polygonMode = vk::PolygonMode::eFill;
			rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
			rasterizer.rasterizerDiscardEnable = false;
			rasterizer.depthBiasEnable = false;
			
			vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			colorBlendAttachment.blendEnable = false;
			
			auto colorBlendAttachments = { colorBlendAttachment };

			vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, false);
			multisampling.sampleShadingEnable = false;
			multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

			vk::PipelineColorBlendStateCreateInfo colorBlending({}, false, {}, 1, colorBlendAttachments.begin());

			auto layouts = { gBufferLayout, lightBufferLayout };

			vk::PipelineLayoutCreateInfo layoutInfo;
			layoutInfo.setLayoutCount = layouts.size();
			layoutInfo.pSetLayouts = layouts.begin();
			layoutInfo.pushConstantRangeCount = 0;

			lightingPipelineLayout = vulkan.device.createPipelineLayout(layoutInfo);

			vk::GraphicsPipelineCreateInfo createInfo;
			createInfo.renderPass = lightingPass;
			createInfo.layout = lightingPipelineLayout;
			createInfo.pColorBlendState = &colorBlending;
			createInfo.pInputAssemblyState = &inputAssembly;
			createInfo.pMultisampleState = &multisampling;
			createInfo.pRasterizationState = &rasterizer;
			createInfo.pStages = shaders.begin();;
			createInfo.stageCount = shaders.size();
			createInfo.pVertexInputState = &vertexInfo;
			createInfo.pViewportState = &viewportState;
			createInfo.pDepthStencilState = nullptr;
			createInfo.pDynamicState = nullptr;

			createInfo.subpass = 0;

			lightingPipeline = vulkan.device.createGraphicsPipeline(nullptr, createInfo);
		
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
			VkUtil::createImage(vulkan, gPositionBuffer, gPositionMemory, extent, gFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
			gPositionView = VkUtil::createImageView(vulkan, gPositionBuffer, gFormat, vk::ImageAspectFlagBits::eColor);
			VkUtil::transitionImageLayout(vulkan, gPositionBuffer, gFormat, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eColorAttachmentOptimal);
	

			VkUtil::createImage(vulkan, gNormalBuffer, gPositionMemory, extent, gFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
			gNormalView = VkUtil::createImageView(vulkan, gNormalBuffer, gFormat, vk::ImageAspectFlagBits::eColor);
			VkUtil::transitionImageLayout(vulkan, gNormalBuffer, gFormat, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eColorAttachmentOptimal);

		}
		
		{
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

		}


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
				swapChainImageViews[i]
			};

			vk::FramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.renderPass = lightingPass;
			framebufferInfo.attachmentCount = 1;
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

			gBufferSampler = vulkan.device.createSampler({});
		}

		// Create descriptor pool
		{
			std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
			poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
			poolSizes[0].descriptorCount = 16;
			poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
			poolSizes[1].descriptorCount = 16;

			vk::DescriptorPoolCreateInfo poolInfo = {};
			poolInfo.poolSizeCount = poolSizes.size();
			poolInfo.pPoolSizes = poolSizes.data();
			poolInfo.maxSets = 4;

			descriptorPool = vulkan.device.createDescriptorPool(poolInfo);	
		}

		// Create descriptor set
		{
			auto layouts = { mvpBufferLayout, lightBufferLayout, gBufferLayout };
			vk::DescriptorSetAllocateInfo allocInfo = {};
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = layouts.size();
			allocInfo.pSetLayouts = layouts.begin();

			auto sets = vulkan.device.allocateDescriptorSets(allocInfo);
			mvpBufferSet = sets[0];
			lightBufferSet = sets[1];
			gBufferSet = sets[2];

			std::vector<vk::WriteDescriptorSet> descriptorWrites;

			{
				// MVP Buffer
				vk::DescriptorBufferInfo bufferInfo = {};
				bufferInfo.buffer = uniformBuffer.buffer;
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(GeneralRenderUniforms);

				descriptorWrites.push_back(vk::WriteDescriptorSet(mvpBufferSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo));
			}
			{
				// Light buffer
				vk::DescriptorBufferInfo bufferInfo = { };
				bufferInfo.buffer = lightUniformBuffer.buffer;
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(Lights);
				
				descriptorWrites.push_back(vk::WriteDescriptorSet(lightBufferSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo));
			}
			{
				// GBuffer
				vk::DescriptorImageInfo posInfo = {};
				posInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				posInfo.imageView = gPositionView;
				posInfo.sampler = gBufferSampler;
				
				vk::DescriptorImageInfo normInfo = {};
				normInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				normInfo.imageView = gNormalView;
				normInfo.sampler = gBufferSampler;

				descriptorWrites.push_back(vk::WriteDescriptorSet(gBufferSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &posInfo, nullptr));
				descriptorWrites.push_back(vk::WriteDescriptorSet(gBufferSet, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &normInfo, nullptr));
			}
			
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
			
			{
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
				commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, geometryPipelineLayout, 0, 1, &mvpBufferSet, 0, nullptr);

				vk::DeviceSize offsets[] = { 0 };

				commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer.buffer, offsets);
				commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
				commandBuffer.drawIndexed(tableMesh.indices.size(), 1, 0, 0, 0);
				commandBuffer.endRenderPass();
				commandBuffer.end();
			}

			// Main render pass
			for (size_t i = 0; i < commandBuffers.size(); i++) {
				auto& commandBuffer = commandBuffers[i];

				vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse, nullptr);
				commandBuffer.begin(beginInfo);

				vk::RenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.renderPass = lightingPass;
				renderPassInfo.framebuffer = swapChainFramebuffers[i];
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = extent;
		
				std::array<vk::ClearValue, 1> clearColors = {};
				clearColors[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.15f, 0.05f, 0.05f, 1.f });

				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = clearColors.data();

				commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, lightingPipeline);
				commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, lightingPipelineLayout, 0, 1, &gBufferSet, 0, nullptr);
				commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, lightingPipelineLayout, 1, 1, &lightBufferSet, 0, nullptr);

				vk::DeviceSize offsets[] = { 0 };
				
				commandBuffer.bindVertexBuffers(0, 1, &screenQuadBuffer.buffer, offsets);
				commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);
				commandBuffer.draw(4, 1, 0, 0);
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


	GeneralRenderUniforms ubo = {};
	
	std::chrono::high_resolution_clock clock;
	auto lastTime = clock.now();
	lights.pointLights[0].intensity = 3;

	ubo.projection = camera.projection;
	ubo.projection[1][1] *= -1;
	while (window.isOpen()) {
		glfwPollEvents();		

		auto now = clock.now();
		float deltaTime = std::chrono::duration<double>(now - lastTime).count();
		lastTime = now;

		// camera
		{
			static bool rightMouseButtonIsDown = false;
			const float cameraSpeed = 15.0 * deltaTime;

			if (glfwGetKey(window.nativeHandle, GLFW_KEY_W) == GLFW_PRESS) {
				camera.transform.position += camera.forwards() * cameraSpeed;
			}
			if (glfwGetKey(window.nativeHandle, GLFW_KEY_S) == GLFW_PRESS) {
				camera.transform.position -= camera.forwards() * cameraSpeed;
			}
			if (glfwGetKey(window.nativeHandle, GLFW_KEY_A) == GLFW_PRESS) {
				camera.transform.position -= camera.right() * cameraSpeed;
			}
			if (glfwGetKey(window.nativeHandle, GLFW_KEY_D) == GLFW_PRESS) {
				camera.transform.position += camera.right() * cameraSpeed;
			}
			if (glfwGetKey(window.nativeHandle, GLFW_KEY_SPACE) == GLFW_PRESS) {
				camera.transform.position += glm::vec3(0, 1, 0) * cameraSpeed;
			}
			if (glfwGetKey(window.nativeHandle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
				camera.transform.position += glm::vec3(0, -1, 0) * cameraSpeed;
			}

			static glm::highp_dvec2 lastMousePos;

			if (glfwGetMouseButton(window.nativeHandle, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS) {
				rightMouseButtonIsDown = true;
				glm::highp_dvec2 mousePos;
				glfwGetCursorPos(window.nativeHandle, &mousePos.x, &mousePos.y);
				camera.yaw += (lastMousePos.x - mousePos.x) / -3;
				camera.pitch += (lastMousePos.y - mousePos.y) / 4;
			}
			else {
				rightMouseButtonIsDown = false;
			}

			glfwGetCursorPos(window.nativeHandle, &lastMousePos.x, &lastMousePos.y);
		}

		// Update uniforms
		{
			static auto startTime = std::chrono::high_resolution_clock().now();
			static auto i = 0;
			i++;

			auto currentTime = std::chrono::high_resolution_clock().now();
			float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.f;

			
			ubo.model = glm::rotate(glm::mat4(), time * glm::radians(90.f), glm::vec3(0.0, 1.0, 0.0));
			ubo.view = camera.getViewMatrix();

			uniformBuffer.fill(&ubo, sizeof(ubo));

			lightUniformBuffer.fill(&lights, sizeof(Lights));

			// 10 seconds passed
			if (time > 10) {
				std::cout << "Rendered " << i << " frames in 10 seconds.\nFPS: " << i / 10 << "\n";
				startTime = std::chrono::high_resolution_clock().now();
				i = 0;
			}
		}


		// Geometry pass
		{
			vk::SubmitInfo submitInfo = {};
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &geometryPassCommandBuffer;
			vulkan.graphicsQueue.submit(1, &submitInfo, nullptr);
		}

		// Lighting pass  
		{
			uint32_t imageIndex = vulkan.device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, nullptr).value;
			
			vk::SubmitInfo submitInfo = {};
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

			// Wait for image to be acquired and signal when render is finished
			vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
			submitInfo.pWaitDstStageMask = waitStages;			
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

			vulkan.graphicsQueue.submit(1, &submitInfo, nullptr);

			// Wait with presenting till rendering has finished
			vk::PresentInfoKHR presentInfo = {};
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &renderFinishedSemaphore;

			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &swapChain;
			presentInfo.pImageIndices = &imageIndex;
			
			vulkan.presentQueue.presentKHR(presentInfo);
		}
	}

	vulkan.device.waitIdle();
	return 0;
}