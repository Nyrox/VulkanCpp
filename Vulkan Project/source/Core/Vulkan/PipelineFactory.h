#pragma once
#include <vulkan/vulkan.hpp>

struct PipelineFactory {
	vk::PipelineLayout layout;
	vk::RenderPass renderPass;

	vk::Pipeline createPipeline(vk::Device device) const;

	vk::PipelineVertexInputStateCreateInfo vertexInput;
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	vk::Viewport viewport;
	vk::Rect2D scissor;
	vk::PipelineRasterizationStateCreateInfo rasterizer;
	vk::PipelineMultisampleStateCreateInfo multisampling;
	vk::PipelineColorBlendStateCreateInfo colorBlending;
	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
};


vk::Pipeline PipelineFactory::createPipeline(vk::Device device) const {
	vk::GraphicsPipelineCreateInfo pipelineInfo;

	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	
	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = layout;
	pipelineInfo.renderPass = renderPass;

	return device.createGraphicsPipeline(nullptr, pipelineInfo);
}