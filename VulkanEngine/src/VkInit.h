#pragma once

#include <GLFW/glfw3.h>

namespace VKE
{
	class VkInit
	{
	public:
		static VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule);
		static VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo();
		static VkPipelineInputAssemblyStateCreateInfo InputAssemblyCreateInfo(VkPrimitiveTopology topology);
		static VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo(VkPolygonMode polygonMode);
		static VkPipelineMultisampleStateCreateInfo MultisamplingStateCreateInfo();
		static VkPipelineColorBlendAttachmentState ColorBlendAttachmentState();
		static VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo();
	};
}