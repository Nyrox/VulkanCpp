#pragma once
#include <Core/Definitions.h>
#include <Core/Vulkan/VulkanInstance.h>
#include <Core/Vulkan/VkUtil.h>

#include <filesystem>
#include <stb/image.h>

namespace std {
	namespace filesystem = experimental::filesystem;
}

constexpr uint32 BINDING_INDEX_MATERIAL = 64;
constexpr uint32 BINDING_INDEX_MATERIAL_SCALARS = 84;

struct Sampler {
	vk::Format format;
	vk::Sampler sampler;
	vk::Image image;
	vk::ImageView imageView;
	vk::DeviceMemory deviceMemory;
	vk::DeviceSize imageSize;
	vk::Extent2D extent;

};

Sampler loadSampler(std::filesystem::path path, VulkanInstance& instance) {
	int texWidth, texHeight, texChannels;
	uint8* pixels = stbi_load(path.string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) throw std::runtime_error("Failed to load texture: " + path.string());

	Sampler sampler;
	sampler.imageSize = texWidth * texHeight * 4;
	sampler.extent = { (uint32)texWidth, (uint32)texHeight };

	vk::Buffer stagingBuffer;
	vk::DeviceMemory stagingBufferMemory;

	
	VkUtil::createBuffer(instance, sampler.imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

	return sampler;
}

class Material {
public:



	vk::DescriptorSetLayout getDescriptorSetLayout(vk::Device device) {
		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		for (int i = 0; i < samplers.size(); i++) {
			vk::DescriptorSetLayoutBinding binding;
			auto& sampler = samplers[i];
			
			binding.binding = BINDING_INDEX_MATERIAL + i;
			binding.descriptorCount = 1;
			binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			binding.pImmutableSamplers = nullptr;
			binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		}
		
		vk::DescriptorSetLayoutBinding scalarLayoutBinding = {};
		scalarLayoutBinding.binding = BINDING_INDEX_MATERIAL_SCALARS;
		scalarLayoutBinding.descriptorCount = 1;
		scalarLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		
		bindings.push_back(scalarLayoutBinding);

		vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.bindingCount = bindings.size();
		layoutInfo.pBindings = bindings.data();

		return device.createDescriptorSetLayout(layoutInfo);
	}



	std::vector<Sampler> samplers;
	std::vector<float> scalars;
};