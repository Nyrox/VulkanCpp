#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;

	static vk::VertexInputBindingDescription& getBindingDescription() {
		static vk::VertexInputBindingDescription bindingDescription = {};

		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.binding = 0;
		bindingDescription.inputRate = vk::VertexInputRate::eVertex;

		return bindingDescription;
	}

	static std::array<vk::VertexInputAttributeDescription, 3>& getAttributeDescriptions() {
		static std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions = {};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
		attributeDescriptions[1].offset = offsetof(Vertex, normal);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}

	static vk::PipelineVertexInputStateCreateInfo getVertexInputState() {
		auto& binding = Vertex::getBindingDescription();
		auto& attributes = Vertex::getAttributeDescriptions();

		vk::PipelineVertexInputStateCreateInfo createInfo;
		createInfo.pVertexAttributeDescriptions = attributes.data();
		createInfo.pVertexBindingDescriptions = &binding;
		createInfo.vertexAttributeDescriptionCount = attributes.size();
		createInfo.vertexBindingDescriptionCount = 1;

		return createInfo;	
	}
};