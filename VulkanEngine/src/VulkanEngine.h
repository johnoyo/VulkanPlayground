#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <set>
#include <vector>
#include <iostream>
#include <optional>
#include <assert.h>

namespace VKE
{
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
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

		const std::vector<const char*> validationLayers =
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

		// Extensions.
		std::vector<const char*> GetRequiredExtensions();
		bool CheckInstanceExtensionSupport(const std::vector<const char*>& sdlExtensionNames);

		// Validation layer check.
		bool CheckValidationLayerSupport();

		// Validation layers debug messages.
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

		// Physical device set up.
		bool IsDeviceSuitable(VkPhysicalDevice device);
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	};
}