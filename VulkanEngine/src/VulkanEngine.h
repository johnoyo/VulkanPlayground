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
#include <unordered_map>
#include <deque>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace VKE
{
	struct Material 
	{
		VkPipeline pipeline;
		VkPipelineLayout pipelineLayout;
	};

	struct RenderObject 
	{
		Mesh* mesh;

		Material* material;

		glm::mat4 transformMatrix;
	};

	struct MeshPushConstants 
	{
		glm::vec4 data;
		glm::mat4 renderMatrix;
	};

	struct GPUCameraData 
	{
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 viewproj;
	};

	struct FrameData 
	{
		VkSemaphore m_PresentSemaphore, m_RenderSemaphore;
		VkFence m_RenderFence;

		VkCommandPool m_CommandPool;
		VkCommandBuffer m_MainCommandBuffer;

		//buffer that holds a single GPUCameraData to use when rendering
		AllocatedBuffer cameraBuffer;

		VkDescriptorSet globalDescriptor;
	};

	//number of frames to overlap when rendering
	constexpr unsigned int FRAME_OVERLAP = 2;

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

	struct DeletionQueue
	{
		std::deque<std::function<void()>> deletors;

		void push_function(std::function<void()>&& function) {
			deletors.push_back(function);
		}

		void flush() {
			// reverse iterate the deletion queue to execute all the functions
			for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
				(*it)(); //call the function
			}

			deletors.clear();
		}
	};

	class VulkanEngine
	{
	public:
		void Init();
		void Run();
		void Draw();
		void Cleanup();

	public:
		std::vector<RenderObject> m_Renderables;

		std::unordered_map<std::string, Material> m_Materials;
		std::unordered_map<std::string, Mesh> m_Meshes;

		//create material and add it to the map
		Material* CreateMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
		//returns nullptr if it can't be found
		Material* GetMaterial(const std::string& name);
		//returns nullptr if it can't be found
		Mesh* GetMesh(const std::string& name);
		//our draw function
		void DrawObjects(VkCommandBuffer cmd, RenderObject* first, int count);

		//frame storage
		FrameData m_Frames[FRAME_OVERLAP];
		//getter for the frame we are rendering to right now.
		FrameData& GetCurrentFrame();

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

		VkImageView m_DepthImageView;
		AllocatedImage m_DepthImage;

		//the format for the depth image
		VkFormat m_DepthFormat;

		VkRenderPass m_RenderPass;
		std::vector<VkFramebuffer> m_FrameBuffers;

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

		DeletionQueue m_MainDeletionQueue;

		VkDescriptorSetLayout m_GlobalSetLayout;
		VkDescriptorPool m_DescriptorPool;

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
		AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
		void CreateDescriptors();

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
		void InitScene();
	};
}