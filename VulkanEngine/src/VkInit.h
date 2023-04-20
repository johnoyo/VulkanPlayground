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

		static VkImageCreateInfo ImageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
		static VkImageViewCreateInfo ImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

		static VkPipelineDepthStencilStateCreateInfo DepthStencilCreateInfo(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);

		static VkRenderPassBeginInfo RenderPassBeginInfo(VkRenderPass renderPass, VkExtent2D extends, VkFramebuffer framebuffer);
	};
}