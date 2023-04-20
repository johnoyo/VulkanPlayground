#pragma once

#include <GLFW/glfw3.h>

#include "VkInit.h"
#include "VkUtils.h"
#include "VkMesh.h"

#include <set>
#include <vector>
#include <fstream>
#include <iostream>
#include <optional>
#include <assert.h>
#include <cstdint>
#include <limits>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace VKE
{
	struct MeshPushConstants 
	{
		glm::vec4 data;
		glm::mat4 renderMatrix;
	};

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails 
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	class PipelineBuilder 
	{
	public:
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;
		VkPipelineVertexInputStateCreateInfo m_VertexInputInfo;
		VkPipelineInputAssemblyStateCreateInfo m_InputAssembly;
		VkViewport m_Viewport;
		VkRect2D m_Scissor;
		VkPipelineRasterizationStateCreateInfo m_Rasterizer;
		VkPipelineColorBlendAttachmentState m_ColorBlendAttachment;
		VkPipelineMultisampleStateCreateInfo m_Multisampling;
		VkPipelineLayout m_PipelineLayout;

		VkPipelineDepthStencilStateCreateInfo m_DepthStencil;

		VkPipeline BuildPipeline(VkDevice device, VkRenderPass pass);
	};

	class VulkanEngine
	{
	public:
		void Init();
		void Run();
		void Draw();
		void Cleanup();

	private:
		GLFWwindow* m_Window;

		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_Device;
		VkSurfaceKHR m_Surface;
		VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;

		VkSwapchainKHR m_SwapChain;
		VkFormat m_SwapChainImageFormat;
		VkExtent2D m_SwapChainExtent;
		std::vector<VkImage> m_SwapChainImages;
		std::vector<VkImageView> m_SwapChainImageViews;

		VkCommandPool m_CommandPool; //the command pool for our commands
		VkCommandBuffer m_MainCommandBuffer; //the buffer we will record into

		VkImageView m_DepthImageView;
		AllocatedImage m_DepthImage;

		//the format for the depth image
		VkFormat m_DepthFormat;

		VkRenderPass m_RenderPass;
		std::vector<VkFramebuffer> m_FrameBuffers;

		VkSemaphore m_PresentSemaphore, m_RenderSemaphore;
		VkFence m_RenderFence;

		uint32_t m_FrameNumber = 0;

		VkPipelineLayout m_TrianglePipelineLayout;
		VkPipeline m_TrianglePipeline;

		VkPipeline m_MeshPipeline;
		VkPipelineLayout m_MeshPipelineLayout;
		Mesh m_TriangleMesh;

		Mesh m_MonkeyMesh;

		VmaAllocator m_Allocator; //vma lib allocator

		const std::vector<const char*> m_DeviceExtensions = 
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		const std::vector<const char*> m_ValidationLayers =
		{
			"VK_LAYER_KHRONOS_validation"
		};

		const bool m_EnableValidationLayers = true;

		void InitWindow();
		void InitVulkan();

		void CreateInstance();
		void SetupDebugMessenger();
		void CreateSurface();
		void SelectPhysicalDevice();
		void CreateLogicalDevice();
		void CreateSwapChain();
		void CreateImageViews();
		void CreateCommands();
		void CreateRenderPass();
		void CreateFrameBuffers();
		void CreateSyncStructures();
		void CreateGraphicsPipeline();
		void CreateAllocator();

		// Extensions.
		std::vector<const char*> GetRequiredExtensions();
		bool CheckInstanceExtensionSupport(const std::vector<const char*>& glfwExtensionNames);

		// Validation layer check.
		bool CheckValidationLayerSupport();

		// Validation layers debug messages.
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

		// Physical device set up.
		bool IsDeviceSuitable(VkPhysicalDevice device);
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

		// Swapchain set up.
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

		void LoadMeshes();
		void UploadMesh(Mesh& mesh);
	};
}