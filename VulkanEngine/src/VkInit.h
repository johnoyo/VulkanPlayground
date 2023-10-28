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

		static VkDescriptorSetLayoutBinding DescriptorsetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
		static VkWriteDescriptorSet WritDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

		static VkImageCreateInfo ImageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
		static VkImageViewCreateInfo ImageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags);

		static VkPipelineDepthStencilStateCreateInfo DepthStencilCreateInfo(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);

		static VkRenderPassBeginInfo RenderPassBeginInfo(VkRenderPass renderPass, VkExtent2D extends, VkFramebuffer framebuffer);

		static VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t graphicsQueueFamilyIndex, VkCommandPoolCreateFlags commandPoolCreateFlags);
		static VkCommandBufferAllocateInfo CommadBufferAllocateInfo(VkCommandPool commandPool, uint32_t commandBufferCount, VkCommandBufferLevel commandBufferLevel);

		static VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlagBits fenceCreateFlagBits);
		static VkFenceCreateInfo FenceCreateInfo();
		static VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags semaphoreCreateFlags);

		static VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
		static VkSubmitInfo SubmitInfo(VkCommandBuffer* cmd);

		static VkSamplerCreateInfo SamplerCreateInfo(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
		static VkWriteDescriptorSet WriteDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);
	};
}