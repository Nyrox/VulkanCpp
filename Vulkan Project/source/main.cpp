
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
#include <Core/Vulkan/PipelineFactory.h>

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

uint32 findMemoryType(vk::PhysicalDevice pDevice, uint32 typeFilter, vk::MemoryPropertyFlags properties) {
	vk::PhysicalDeviceMemoryProperties memProperties = pDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
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

vk::ShaderModule createShaderModule(const std::vector<int8>& code, vk::Device device) {
	vk::ShaderModuleCreateInfo createInfo = {};
	createInfo.codeSize = code.size();

	std::vector<uint32> codeAligned(code.size() / sizeof(uint32_t) + 1);
	memcpy(codeAligned.data(), code.data(), code.size());
	createInfo.pCode = codeAligned.data();

	return device.createShaderModule(createInfo);
}

int main() {
	Window window(1280, 720, "Praise kek");
	Camera camera(Transform(glm::vec3(0, 5, 3)), glm::perspective(glm::radians(75.0f), 1280.f / 720.f, 0.1f, 100.0f));

	Mesh tableMesh = MeshLoaders::load_ply("meshes/UnitCube.ply");
	Material pbrMaterial;


	Lights lights;
	lights.pointLights[0].position = glm::vec3(-2, 5, 0);
	lights.pointLights[1].position = glm::vec3(-4, -2, 1);

	lights.pointLightCount = 2;

	vk::Format format;
	vk::Extent2D extent;
	vk::Viewport screenViewport;
	vk::Rect2D screenScissor;

	vk::SwapchainKHR swapChain;

	std::vector<vk::Image> swapChainImages;
	std::vector<vk::ImageView> swapChainImageViews;


	VulkanInstance vulkan;
	vulkan.createVulkanInstance();
	vulkan.createSurface(window.nativeHandle);
	vulkan.createPhysicalDevice();
	vulkan.createLogicalDevice();
	vulkan.createUtilityPool();
	auto si = createSwapChain(vulkan, vulkan.instance, swapChain, vulkan.physicalDevice, vulkan.surface, vulkan.device, swapChainImages);
	format = std::get<vk::Format>(si);
	extent = std::get<vk::Extent2D>(si);

	screenViewport.width = extent.width;
	screenViewport.height = extent.height;
	screenViewport.maxDepth = 1.0f;

	screenScissor.extent = extent;

	createImageViews(vulkan, swapChainImageViews, swapChainImages, format);




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



	Mesh unitCube = MeshLoaders::load_ply("meshes/UnitCube.ply");
	DeviceLocalBuffer unitCubeVertexBuffer(vulkan, vk::BufferUsageFlagBits::eVertexBuffer);
	DeviceLocalBuffer unitCubeIndexBuffer(vulkan, vk::BufferUsageFlagBits::eIndexBuffer);

	unitCubeVertexBuffer.fill(unitCube.vertices.data(), sizeof(Vertex) * unitCube.vertices.size());
	unitCubeIndexBuffer.fill(unitCube.indices.data(), sizeof(uint32) * unitCube.indices.size());

	/*
		Refactor
	*/

	vk::DescriptorSetLayout gBufferLayout;
	vk::DescriptorSetLayout mvpBufferLayout;
	vk::DescriptorSetLayout lightBufferLayout;

	vk::DescriptorSet gBufferSet, mvpBufferSet, lightBufferSet;
	vk::Sampler gBufferSampler;


	struct RenderPipeline {
		vk::RenderPass renderPass;
		vk::PipelineLayout layout;
		vk::Pipeline pipeline;
	};

	struct CubemapRenderTarget {
		std::array<vk::ImageView, 6> imageViews;
		std::array<vk::Framebuffer, 6> framebuffers;
	};

	struct Cubemap {
		~Cubemap() { delete renderTarget; }

		vk::ImageView view;
		vk::Image image;
		vk::DeviceMemory memory;
		
		CubemapRenderTarget* renderTarget = nullptr;
	};

	Cubemap environmentCubemap;

	vk::CommandBuffer cubemapCommandBuffer;


	vk::Sampler plainSampler;
	{
		vk::SamplerCreateInfo samplerInfo;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;

		plainSampler = vulkan.device.createSampler(samplerInfo);
	}
	
	vk::DescriptorSetLayout skyboxSetLayout;
	vk::DescriptorSet skyboxSet;

	vk::Pipeline skyboxPipeline;
	vk::PipelineLayout skyboxPipelineLayout;
	vk::RenderPass skyboxPass;

	try {
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
			poolInfo.maxSets = 8;

			descriptorPool = vulkan.device.createDescriptorPool(poolInfo);
		}

		// Create command pool
		{
			auto indices = vulkan.findQueueFamilyIndices(vulkan.physicalDevice);

			vk::CommandPoolCreateInfo poolInfo = {};
			poolInfo.queueFamilyIndex = indices.graphicsFamily;
			commandPool = vulkan.device.createCommandPool(poolInfo);
		}

		// Create depth buffer
		{
			vk::Format depthFormat = vk::Format::eD32Sfloat;
			VkUtil::createImage(vulkan, depthImage, depthImageMemory, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
			depthImageView = VkUtil::createImageView(vulkan, depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
			VkUtil::transitionImageLayout(vulkan, depthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
		}

		// Bake Environment Map Into Cubemap
		{
			vk::RenderPass renderPass;
			vk::PipelineLayout pipelineLayout;
			vk::Pipeline pipeline;

			vk::DescriptorSetLayout descSetLayout;
			vk::DescriptorSet descSet;

			vk::CommandBuffer commandBuffer;

			struct {
				vk::Image image;
				vk::ImageView view;
				vk::DeviceMemory memory;
			} sourceImage;

			// Load source image
			{
				int texWidth, texHeight, texChannels;
				uint8* pixels = stbi_load("textures/waterfall_Bg.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
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

				VkUtil::createImage(vulkan, sourceImage.image, sourceImage.memory, imgExtent, vk::Format::eR8G8B8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
				VkUtil::transitionImageLayout(vulkan, sourceImage.image, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eTransferDstOptimal);
				VkUtil::copyBufferToImage(vulkan, stagingBuffer, sourceImage.image, imgExtent);
				VkUtil::transitionImageLayout(vulkan, sourceImage.image, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

				vulkan.device.destroyBuffer(stagingBuffer);
				vulkan.device.freeMemory(stagingBufferMemory);

				sourceImage.view = VkUtil::createImageView(vulkan, sourceImage.image, vk::Format::eR8G8B8A8Unorm, vk::ImageAspectFlagBits::eColor);
			}

			// Create Descriptor Set Layout
			{
				vk::DescriptorSetLayoutBinding envMap;
				envMap.binding = 0;
				envMap.descriptorCount = 1;
				envMap.descriptorType = vk::DescriptorType::eCombinedImageSampler;
				envMap.stageFlags = vk::ShaderStageFlagBits::eFragment;

				vk::DescriptorSetLayoutCreateInfo layoutInfo;
				layoutInfo.bindingCount = 1;
				layoutInfo.pBindings = &envMap;

				descSetLayout = vulkan.device.createDescriptorSetLayout(layoutInfo);
			}

			// Create Descriptor Set
			{
				vk::DescriptorSetAllocateInfo allocInfo;
				allocInfo.descriptorPool = descriptorPool;
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &descSetLayout;

				descSet = vulkan.device.allocateDescriptorSets(allocInfo)[0];

				vk::DescriptorImageInfo imageInfo;
				imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				imageInfo.imageView = sourceImage.view;
				imageInfo.sampler = plainSampler;

				vk::WriteDescriptorSet descWrite;
				descWrite.descriptorCount = 1;
				descWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
				descWrite.dstBinding = 0;
				descWrite.dstArrayElement = 0;
				descWrite.dstSet = descSet;
				descWrite.pImageInfo = &imageInfo;

				vulkan.device.updateDescriptorSets(1, &descWrite, 0, nullptr);
			}

			// Create Renderpass
			{
				// 1 Attachment for the face we are rendering in to
				// 1 Depth Attachment
				std::array<vk::AttachmentDescription, 2> attachments;
				for (auto& it : attachments) {
					it.samples = vk::SampleCountFlagBits::e1;
					it.initialLayout = vk::ImageLayout::eUndefined;
				}

				attachments[0].format = vk::Format::eR8G8B8A8Unorm;
				attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
				attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
				attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
				attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
				attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

				attachments[1].format = vk::Format::eD32Sfloat;
				attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
				attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
				attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eClear;
				attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
				attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

				std::array<vk::AttachmentReference, 2> attachmentRefs;
				for (int i = 0; i < attachmentRefs.size(); i++) {
					attachmentRefs[i].attachment = i;
				}

				attachmentRefs[0].layout = vk::ImageLayout::eColorAttachmentOptimal;
				attachmentRefs[1].layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

				vk::SubpassDescription subpass;
				subpass.colorAttachmentCount = 1;
				subpass.pColorAttachments = &attachmentRefs[0];
				subpass.pDepthStencilAttachment = &attachmentRefs[1];

				vk::SubpassDependency dependency;
				dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
				dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

				vk::RenderPassCreateInfo renderPassInfo;
				renderPassInfo.attachmentCount = attachments.size();
				renderPassInfo.pAttachments = attachments.data();
				renderPassInfo.subpassCount = 1;
				renderPassInfo.pSubpasses = &subpass;
				renderPassInfo.dependencyCount = 1;
				renderPassInfo.pDependencies = &dependency;

				renderPass = vulkan.device.createRenderPass(renderPassInfo);
			}

			// Create Pipeline
			{
				{
					auto vertexShaderModule = createShaderModule(FUtil::file_read_binary("shaders/compiled/process/equi_to_cube.vert.spv"), vulkan.device);
					auto fragmentShaderModule = createShaderModule(FUtil::file_read_binary("shaders/compiled/process/equi_to_cube.frag.spv"), vulkan.device);

					PipelineFactory factory;
					factory.shaderStages.emplace_back(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main");
					factory.shaderStages.emplace_back(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main");

					auto bindingDescription = Vertex::getBindingDescription();
					auto attributeDescriptions = Vertex::getAttributeDescriptions();

					factory.vertexInput.vertexBindingDescriptionCount = 1;
					factory.vertexInput.pVertexBindingDescriptions = &bindingDescription;
					factory.vertexInput.vertexAttributeDescriptionCount = attributeDescriptions.size();
					factory.vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

					factory.inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
					factory.viewport = screenViewport;
					factory.scissor = screenScissor;

					factory.rasterizer.lineWidth = 1.0f;

					vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
					colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB;
					auto colorBlendAttachments = { colorBlendAttachment };

					factory.colorBlending.attachmentCount = 1;
					factory.colorBlending.pAttachments = colorBlendAttachments.begin();

					factory.multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

					factory.depthStencil.depthTestEnable = true;
					factory.depthStencil.depthWriteEnable = true;
					factory.depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;

					auto setLayouts = { descSetLayout };

					std::array<vk::PushConstantRange, 1> pushConstants;
					pushConstants[0].offset = 0;
					pushConstants[0].size = sizeof(glm::mat4) * 2;
					pushConstants[0].stageFlags = vk::ShaderStageFlagBits::eVertex;

					vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {};
					pipelineLayoutInfo.setLayoutCount = setLayouts.size();
					pipelineLayoutInfo.pSetLayouts = setLayouts.begin();
					pipelineLayoutInfo.pushConstantRangeCount = pushConstants.size();
					pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

					pipelineLayout = vulkan.device.createPipelineLayout(pipelineLayoutInfo);

					factory.layout = pipelineLayout;
					factory.renderPass = renderPass;
					pipeline = factory.createPipeline(vulkan.device);
				}
			}

			// Create cubemap
			{
				vk::ImageCreateInfo imageInfo;
				imageInfo.imageType = vk::ImageType::e2D;
				imageInfo.extent = { (uint32)extent.width, (uint32)extent.height, 1 };
				imageInfo.mipLevels = 1;
				imageInfo.arrayLayers = 6;
				imageInfo.format = vk::Format::eR8G8B8A8Unorm;
				imageInfo.initialLayout = vk::ImageLayout::ePreinitialized;
				imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
				imageInfo.samples = vk::SampleCountFlagBits::e1;
				environmentCubemap.image = vulkan.device.createImage(imageInfo);

				vk::MemoryRequirements memReq = vulkan.device.getImageMemoryRequirements(environmentCubemap.image);
				vk::MemoryAllocateInfo allocInfo;
				allocInfo.allocationSize = memReq.size;
				allocInfo.memoryTypeIndex = findMemoryType(vulkan.physicalDevice, memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

				environmentCubemap.memory = vulkan.device.allocateMemory(allocInfo);
				vulkan.device.bindImageMemory(environmentCubemap.image, environmentCubemap.memory, 0);

				VkUtil::transitionImageLayout(vulkan, environmentCubemap.image, vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eColorAttachmentOptimal);

				{
					vk::ImageViewCreateInfo viewInfo;
					viewInfo.image = environmentCubemap.image;
					viewInfo.viewType = vk::ImageViewType::e3D;
					viewInfo.format = vk::Format::eR8G8B8A8Unorm;
					viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
					viewInfo.subresourceRange.levelCount = 1;
					viewInfo.subresourceRange.layerCount = 1;

					environmentCubemap.view = vulkan.device.createImageView(viewInfo);
				}

				environmentCubemap.renderTarget = new CubemapRenderTarget();

				for (int i = 0; i < 6; i++) {
					vk::ImageViewCreateInfo viewInfo;
					viewInfo.image = environmentCubemap.image;
					viewInfo.viewType = vk::ImageViewType::e2D;
					viewInfo.format = vk::Format::eR8G8B8A8Unorm;
					viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
					viewInfo.subresourceRange.levelCount = 1;
					viewInfo.subresourceRange.layerCount = 1;
					viewInfo.subresourceRange.baseArrayLayer = i;

					environmentCubemap.renderTarget->imageViews[i] = vulkan.device.createImageView(viewInfo);

					auto attachments = {
						environmentCubemap.renderTarget->imageViews[i],
						depthImageView
					};

					vk::FramebufferCreateInfo fbInfo;
					fbInfo.attachmentCount = attachments.size();
					fbInfo.pAttachments = attachments.begin();
					fbInfo.height = extent.height;
					fbInfo.width = extent.width;
					fbInfo.layers = 1;
					fbInfo.renderPass = renderPass;

					environmentCubemap.renderTarget->framebuffers[i] = vulkan.device.createFramebuffer(fbInfo);
				}
			}


			// Create command buffer
			{
				vk::CommandBufferAllocateInfo allocInfo;
				allocInfo.commandPool = commandPool;
				allocInfo.level = vk::CommandBufferLevel::ePrimary;
				allocInfo.commandBufferCount = 1;
				commandBuffer = vulkan.device.allocateCommandBuffers(allocInfo)[0];
			}

			// Record commands
			{
				auto& cb = commandBuffer;
				cb.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));

				std::array<vk::ClearValue, 2> clearColors = {};
				clearColors[0].color = vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f });
				clearColors[1].depthStencil = vk::ClearDepthStencilValue(1, 0);

				glm::mat4 captureViews[] =
				{
					glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
					glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
					glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
					glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
					glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
					glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
				};
				glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
				captureProjection[1][1] *= -1;

				for (int i = 0; i < 6; i++) {

					vk::RenderPassBeginInfo rInfo;
					rInfo.renderPass = renderPass;
					rInfo.framebuffer = environmentCubemap.renderTarget->framebuffers[i];
					rInfo.clearValueCount = clearColors.size();
					rInfo.pClearValues = clearColors.data();
					rInfo.renderArea = vk::Rect2D({ 0, 0, }, extent);

					cb.beginRenderPass(rInfo, vk::SubpassContents::eInline);

					cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
					cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descSet, 0, nullptr);

					vk::DeviceSize offsets[] = { 0 };
					cb.bindVertexBuffers(0, 1, &unitCubeVertexBuffer.buffer, offsets);
					cb.bindIndexBuffer(unitCubeIndexBuffer.buffer, 0, vk::IndexType::eUint32);

					glm::mat4 pushConstants[] = { captureViews[i], captureProjection };

					cb.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4) * 2, pushConstants);
					cb.drawIndexed(unitCube.indices.size(), 1, 0, 0, 0);

					cb.endRenderPass();
				}


				cb.end();
			}
		
			vk::SubmitInfo submitInfo;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;
			
			vulkan.graphicsQueue.submit(1, &submitInfo, nullptr);
		}

		// Create descriptor set layouts 

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

		{
			vk::DescriptorSetLayoutBinding skybox = {};
			skybox.binding = 0;
			skybox.descriptorCount = 1;
			skybox.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			skybox.stageFlags = vk::ShaderStageFlagBits::eFragment;

			vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &skybox);
			skyboxSetLayout = vulkan.device.createDescriptorSetLayout(layoutInfo);
		}

		/* Create deferred render pipeline */
		geometryVertexShader	= createShaderModule(FUtil::file_read_binary("shaders/compiled/deferred/geometry_pass.vert.spv"), vulkan.device);
		geometryFragmentShader	= createShaderModule(FUtil::file_read_binary("shaders/compiled/deferred/geometry_pass.frag.spv"), vulkan.device);

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
			depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
			depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
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
			PipelineFactory factory;
			factory.shaderStages.emplace_back(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, geometryVertexShader, "main");
			factory.shaderStages.emplace_back(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, geometryFragmentShader, "main");

			auto bindingDescription = Vertex::getBindingDescription();
			auto attributeDescriptions = Vertex::getAttributeDescriptions();

			factory.vertexInput.vertexBindingDescriptionCount = 1;
			factory.vertexInput.pVertexBindingDescriptions = &bindingDescription;
			factory.vertexInput.vertexAttributeDescriptionCount = attributeDescriptions.size();
			factory.vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

			factory.inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
			factory.viewport = screenViewport;
			factory.scissor = screenScissor;

			factory.rasterizer.lineWidth = 1.0f;
			factory.depthStencil.depthTestEnable = true;
			factory.depthStencil.depthWriteEnable = true;

			vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB;
			auto colorBlendAttachments = { colorBlendAttachment, colorBlendAttachment };

			factory.colorBlending.attachmentCount = 2;
			factory.colorBlending.pAttachments = colorBlendAttachments.begin();

			factory.multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

			factory.depthStencil.depthTestEnable = true;
			factory.depthStencil.depthWriteEnable = true;
			factory.depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;


			auto setLayouts = { mvpBufferLayout };

			vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {};
			pipelineLayoutInfo.setLayoutCount = setLayouts.size();
			pipelineLayoutInfo.pSetLayouts = setLayouts.begin();
			geometryPipelineLayout = vulkan.device.createPipelineLayout(pipelineLayoutInfo);

			factory.layout = geometryPipelineLayout;
			factory.renderPass = geometryPass;
			geometryPipeline = factory.createPipeline(vulkan.device);
		}


		/* Create lighting pass render pipeline */
		lightingVertexShader	= createShaderModule(FUtil::file_read_binary("shaders/compiled/deferred/lighting_pass.vert.spv"), vulkan.device);
		lightingFragmentShader	= createShaderModule(FUtil::file_read_binary("shaders/compiled/deferred/lighting_pass.frag.spv"), vulkan.device);

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

			vk::AttachmentDescription depthAttachment;
			depthAttachment.format = vk::Format::eD32Sfloat;
			depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
			depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
			depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
			depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
			depthAttachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
			depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
			depthAttachment.samples = vk::SampleCountFlagBits::e1;

			vk::AttachmentReference colorAttachmentRef;
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::AttachmentReference depthAttachmentRef;
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

			vk::SubpassDescription subpass;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;

			auto attachments = { colorAttachment, depthAttachment };

			vk::RenderPassCreateInfo renderPassInfo;
			renderPassInfo.attachmentCount = 2;
			renderPassInfo.pAttachments = attachments.begin();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;

			lightingPass = vulkan.device.createRenderPass(renderPassInfo);
		}

		/* Create lighting pipeline */
		{
			PipelineFactory factory;

			factory.shaderStages.emplace_back(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, lightingVertexShader, "main");
			factory.shaderStages.emplace_back(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, lightingFragmentShader, "main");

			factory.vertexInput = Vertex::getVertexInputState();

			factory.inputAssembly.topology = vk::PrimitiveTopology::eTriangleStrip;
			vk::Viewport viewport;
			viewport.width = extent.width;
			viewport.height = extent.height;
			viewport.maxDepth = 1.0f;
			viewport.x = 0;
			viewport.y = 0;

			vk::Rect2D scissor;
			scissor.extent = extent;
			scissor.offset = { 0, 0, };

			factory.viewport = screenViewport;
			factory.scissor = screenScissor;

			factory.rasterizer.lineWidth = 1.0f;


			vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			colorBlendAttachment.blendEnable = false;

			auto colorBlendAttachments = { colorBlendAttachment };
			factory.colorBlending.attachmentCount = colorBlendAttachments.size();
			factory.colorBlending.pAttachments = colorBlendAttachments.begin();

			factory.multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

			auto layouts = { gBufferLayout, lightBufferLayout };
			vk::PipelineLayoutCreateInfo layoutInfo;
			layoutInfo.setLayoutCount = layouts.size();
			layoutInfo.pSetLayouts = layouts.begin();

			lightingPipelineLayout = vulkan.device.createPipelineLayout(layoutInfo);

			factory.layout = lightingPipelineLayout;
			factory.renderPass = lightingPass;

			lightingPipeline = factory.createPipeline(vulkan.device);
		}

		// Create skybox pipeline
		/* Create render pass */
		{
			vk::AttachmentDescription colorAttachment;
			colorAttachment.format = format;
			colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
			colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
			colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
			colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
			colorAttachment.initialLayout = vk::ImageLayout::ePresentSrcKHR;
			colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;
			colorAttachment.samples = vk::SampleCountFlagBits::e1;

			vk::AttachmentDescription depthAttachment;
			depthAttachment.format = vk::Format::eD32Sfloat;
			depthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
			depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
			depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
			depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
			depthAttachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
			depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

			vk::AttachmentReference colorAttachmentRef;
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::AttachmentReference depthAttachmentRef;
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

			auto attachments = { colorAttachment, depthAttachment };

			vk::SubpassDescription subpass;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;
			

			vk::RenderPassCreateInfo renderPassInfo;
			renderPassInfo.attachmentCount = 2;
			renderPassInfo.pAttachments = attachments.begin();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;

			skyboxPass = vulkan.device.createRenderPass(renderPassInfo);
		}
		{
			PipelineFactory factory;
			factory.shaderStages.emplace_back(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, createShaderModule(FUtil::file_read_binary("shaders/compiled/forward/skybox.vert.spv"), vulkan.device), "main");
			factory.shaderStages.emplace_back(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, createShaderModule(FUtil::file_read_binary("shaders/compiled/forward/skybox.frag.spv"), vulkan.device), "main");
			
			factory.vertexInput = Vertex::getVertexInputState();

			factory.inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
			factory.viewport = screenViewport;
			factory.scissor = screenScissor;
			factory.rasterizer.lineWidth = 1.0f;

			factory.depthStencil.depthTestEnable = true;
			factory.depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;

			vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			colorBlendAttachment.blendEnable = false;

			auto colorBlendAttachments = { colorBlendAttachment };
			factory.colorBlending.attachmentCount = colorBlendAttachments.size();
			factory.colorBlending.pAttachments = colorBlendAttachments.begin();

			factory.multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

			auto layouts = { mvpBufferLayout, skyboxSetLayout };
			vk::PipelineLayoutCreateInfo layoutInfo;
			layoutInfo.setLayoutCount = layouts.size();
			layoutInfo.pSetLayouts = layouts.begin();

			skyboxPipelineLayout = vulkan.device.createPipelineLayout(layoutInfo);

			factory.layout = skyboxPipelineLayout;
			factory.renderPass = skyboxPass;

			skyboxPipeline = factory.createPipeline(vulkan.device);

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






		// Create framebuffers
		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vk::ImageView attachments[] = {
				swapChainImageViews[i],
				depthImageView
			};

			vk::FramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.renderPass = lightingPass;
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

			//// Create texture sampler
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
			//
			textureImageSampler = vulkan.device.createSampler(samplerInfo);

			gBufferSampler = vulkan.device.createSampler({});
		}



		// Create descriptor set
		{
			auto layouts = { mvpBufferLayout, lightBufferLayout, gBufferLayout, skyboxSetLayout };
			vk::DescriptorSetAllocateInfo allocInfo = {};
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = layouts.size();
			allocInfo.pSetLayouts = layouts.begin();

			auto sets = vulkan.device.allocateDescriptorSets(allocInfo);
			mvpBufferSet = sets[0];
			lightBufferSet = sets[1];
			gBufferSet = sets[2];
			skyboxSet = sets[3];

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

			{
				// Env cubemap
				vk::DescriptorImageInfo envMapInfo;
				envMapInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				envMapInfo.imageView = environmentCubemap.view;
				envMapInfo.sampler = plainSampler;

				descriptorWrites.push_back(vk::WriteDescriptorSet(skyboxSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &envMapInfo, nullptr));
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


			vk::RenderPassBeginInfo fwdPass;
			fwdPass.renderPass = skyboxPass;
			fwdPass.framebuffer = swapChainFramebuffers[i];
			fwdPass.renderArea = renderPassInfo.renderArea;
			
			commandBuffer.beginRenderPass(fwdPass, vk::SubpassContents::eInline);
			commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, skyboxPipeline);
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, skyboxPipelineLayout, 0, 1, &mvpBufferSet, 0, nullptr);
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, skyboxPipelineLayout, 1, 1, &skyboxSet, 0, nullptr);
			commandBuffer.bindVertexBuffers(0, 1, &unitCubeVertexBuffer.buffer, offsets);
			commandBuffer.bindIndexBuffer(unitCubeIndexBuffer.buffer, 0, vk::IndexType::eUint32);
			commandBuffer.drawIndexed(unitCube.indices.size(), 1, 0, 0, 0);
			commandBuffer.endRenderPass();
			commandBuffer.end();
		}

		// Create semaphores
		vk::SemaphoreCreateInfo semaphoreInfo = {};
		imageAvailableSemaphore = vulkan.device.createSemaphore(semaphoreInfo);
		renderFinishedSemaphore = vulkan.device.createSemaphore(semaphoreInfo);

	}	
	catch (std::runtime_error& error) {
		std::cout << error.what() << "\n";
		abort();
	}

	glfwSetInputMode(window.nativeHandle, GLFW_STICKY_KEYS, 1);

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

		static bool r = true;
		if (glfwGetKey(window.nativeHandle, GLFW_KEY_Q) == GLFW_RELEASE) {
			r = !r;
		}

		
		GeneralRenderUniforms ubo;

		// Cubemapping
		//{
		//	vk::SubmitInfo submitInfo;
		//	submitInfo.commandBufferCount = 1;
		//	submitInfo.pCommandBuffers = &cubemapCommandBuffer;
		//	vulkan.graphicsQueue.submit(1, &submitInfo, nullptr);
		//}


		// Update uniforms
		{
			static auto startTime = std::chrono::high_resolution_clock().now();
			static auto i = 0;
			i++;

			auto currentTime = std::chrono::high_resolution_clock().now();
			float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.f;

			ubo.model = glm::rotate(glm::mat4(), 1.f, glm::vec3(0.0, 1.0, 0.0));
			
			ubo.view = camera.getViewMatrix();
			ubo.projection = camera.projection;
			ubo.projection[1][1] *= -1;

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
		vulkan.graphicsQueue.waitIdle();
		uint32_t imageIndex = vulkan.device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, nullptr).value;


		// Lighting pass  
		{
			
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
		}

		// Wait with presenting till rendering has finished
		vk::PresentInfoKHR presentInfo = {};
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphore;

		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageIndex;

		vulkan.presentQueue.presentKHR(presentInfo);

		// Required to prevent Lunar SDK Validation Layers from leaking about 1mb/s of memory...
		if (enableValidationLayers) {
			vulkan.graphicsQueue.waitIdle();
		}
	}

	vulkan.device.waitIdle();
	return 0;
}