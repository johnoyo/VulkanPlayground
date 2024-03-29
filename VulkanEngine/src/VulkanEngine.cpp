#include "VulkanEngine.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

#include "VkUtils.h"

namespace VKE
{
	void VulkanEngine::Init()
	{
		InitWindow();
		InitVulkan();

		LoadImages();
		LoadMeshes();

		InitScene();
	}

	void VulkanEngine::Run()
	{
		while (!glfwWindowShouldClose(m_Window)) 
		{
			Draw();

			glfwPollEvents();
		}
	}

	void VulkanEngine::Draw()
	{
		VkResult result;

		// Wait until the GPU has finished rendering the last frame. Timeout of 1 second
		result = vkWaitForFences(m_Device, 1, &GetCurrentFrame().m_RenderFence, true, 1000000000);
		assert(result == VK_SUCCESS);

		result = vkResetFences(m_Device, 1, &GetCurrentFrame().m_RenderFence);
		assert(result == VK_SUCCESS);

		uint32_t swapchainImageIndex;
		result = vkAcquireNextImageKHR(m_Device, m_SwapChain, 1000000000, GetCurrentFrame().m_PresentSemaphore, nullptr, &swapchainImageIndex);
		assert(result == VK_SUCCESS);

		// Now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
		result = vkResetCommandBuffer(GetCurrentFrame().m_MainCommandBuffer, 0);
		assert(result == VK_SUCCESS);

		VkCommandBuffer cmd = GetCurrentFrame().m_MainCommandBuffer;

		// Begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
		VkCommandBufferBeginInfo cmdBeginInfo = {};
		cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBeginInfo.pNext = nullptr;

		cmdBeginInfo.pInheritanceInfo = nullptr;
		cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		result = vkBeginCommandBuffer(cmd, &cmdBeginInfo);
		assert(result == VK_SUCCESS);

		// Make a clear-color from frame number. This will flash with a 120*pi frame period.
		VkClearValue clearValue;
		// float flash = std::abs(std::sin(m_FrameNumber / 120.f));
		clearValue.color = { { 0.0f, 0.0f, 0.f, 1.0f } };

		//clear depth at 1
		VkClearValue depthClear;
		depthClear.depthStencil.depth = 1.f;

		// Start the main renderpass.
		// We will use the clear color from above, and the framebuffer of the index the swapchain gave us
		VkRenderPassBeginInfo rpInfo = VkInit::RenderPassBeginInfo(m_RenderPass, m_SwapChainExtent, m_FrameBuffers[swapchainImageIndex]);

		//connect clear values
		rpInfo.clearValueCount = 2;

		VkClearValue clearValues[] = { clearValue, depthClear };

		rpInfo.pClearValues = &clearValues[0];

		vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

		DrawObjects(cmd, m_Renderables.data(), m_Renderables.size());

		//finalize the render pass
		vkCmdEndRenderPass(cmd);

		//finalize the command buffer (we can no longer add commands, but it can now be executed)
		result = vkEndCommandBuffer(cmd);
		assert(result == VK_SUCCESS);

		//prepare the submission to the queue.
		//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
		//we will signal the _renderSemaphore, to signal that rendering has finished

		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.pNext = nullptr;

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		submit.pWaitDstStageMask = &waitStage;

		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &GetCurrentFrame().m_PresentSemaphore;

		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &GetCurrentFrame().m_RenderSemaphore;

		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cmd;

		//submit command buffer to the queue and execute it.
		// _renderFence will now block until the graphic commands finish execution
		result = vkQueueSubmit(m_GraphicsQueue, 1, &submit, GetCurrentFrame().m_RenderFence);
		assert(result == VK_SUCCESS);

		// this will put the image we just rendered into the visible window.
		// we want to wait on the _renderSemaphore for that,
		// as it's necessary that drawing commands have finished before the image is displayed to the user
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;

		presentInfo.pSwapchains = &m_SwapChain;
		presentInfo.swapchainCount = 1;

		presentInfo.pWaitSemaphores = &GetCurrentFrame().m_RenderSemaphore;
		presentInfo.waitSemaphoreCount = 1;

		presentInfo.pImageIndices = &swapchainImageIndex;

		result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
		assert(result == VK_SUCCESS);

		//increase the number of frames drawn
		m_FrameNumber++;
	}

	void VulkanEngine::Cleanup()
	{
		vkDeviceWaitIdle(m_Device);

		m_MainDeletionQueue.flush();

		vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);

		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

		vkDestroyPipelineLayout(m_Device, m_TrianglePipelineLayout, nullptr);

		vkDestroyPipelineLayout(m_Device, m_MeshPipelineLayout, nullptr);

		vkDestroyPipeline(m_Device, m_TrianglePipeline, nullptr);

		vkDestroyPipeline(m_Device, m_MeshPipeline, nullptr);

		for (int i = 0; i < m_FrameBuffers.size(); i++) 
		{
			vkDestroyFramebuffer(m_Device, m_FrameBuffers[i], nullptr);
			vkDestroyImageView(m_Device, m_SwapChainImageViews[i], nullptr);
		}

		for (auto& texture : m_LoadedTextures)
		{
			vkDestroyImageView(m_Device, texture.second.ImageView, nullptr);
		}

		vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
		vmaDestroyImage(m_Allocator, m_DepthImage.Image, m_DepthImage.Allocation);

		vmaDestroyAllocator(m_Allocator);

		vkDestroyDevice(m_Device, nullptr);
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

		if (m_EnableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
		}

		vkDestroyInstance(m_Instance, nullptr);

		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	void VulkanEngine::InitWindow() 
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		m_Window = glfwCreateWindow(800, 600, "VulkanApp", nullptr, nullptr);
	}

	void VulkanEngine::InitVulkan()
	{
		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		SelectPhysicalDevice();
		CreateLogicalDevice();

		CreateAllocator();

		CreateSwapChain();
		CreateImageViews();
		CreateCommands();
		CreateRenderPass();
		CreateFrameBuffers();
		CreateSyncStructures();
		CreateDescriptors();
		CreateGraphicsPipeline();
	}

	void VulkanEngine::CreateInstance()
	{
		if (m_EnableValidationLayers && !CheckValidationLayerSupport())
		{
			assert(false); // Validation layers requested, but not available!
		}

		// Application Info
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle!";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Vulkan Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_1;

		// Instance info
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// Get extensions.
		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		// Validation layers.
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
		if (m_EnableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = m_ValidationLayers.data();

			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);

		assert(result == VK_SUCCESS);
	}

	void VulkanEngine::SetupDebugMessenger()
	{
		if (!m_EnableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		PopulateDebugMessengerCreateInfo(createInfo);

		VkResult result = CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger);
		assert(result == VK_SUCCESS); // Failed to set up debug messenger!
	}

	void VulkanEngine::CreateSurface()
	{
		VkResult result = glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface);
		assert(result == VK_SUCCESS);
	}

	void VulkanEngine::SelectPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			assert(false); // Failed to find GPUs with Vulkan support!
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

		m_PhysicalDevice = VK_NULL_HANDLE;

		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device))
			{
				m_PhysicalDevice = device;
				break;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
		{
			assert(false); // Failed to find a suitable GPU!
		}

		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_GpuProperties);
		std::cout << "The GPU has a minimum buffer alignment of " << m_GpuProperties.limits.minUniformBufferOffsetAlignment << std::endl;
	}

	void VulkanEngine::CreateLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
		createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

		if (m_EnableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
		assert(result == VK_SUCCESS); // Failed to create logical device!

		vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
	}

	void VulkanEngine::CreateSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.Formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.PresentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.Capabilities);

		uint32_t imageCount = swapChainSupport.Capabilities.minImageCount + 1;

		if (swapChainSupport.Capabilities.maxImageCount > 0 && imageCount > swapChainSupport.Capabilities.maxImageCount) 
		{
			imageCount = swapChainSupport.Capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily) 
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else 
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		createInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		VkResult result = vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain);
		assert(result == VK_SUCCESS); // Failed to create swapchain!

		// Retrieving the swap chain images.
		vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent = extent;

		//depth image size will match the window
		VkExtent3D depthImageExtent = 
		{
			m_SwapChainExtent.width,
			m_SwapChainExtent.height,
			1
		};

		//hardcoding the depth format to 32 bit float
		m_DepthFormat = VK_FORMAT_D32_SFLOAT;

		//the depth image will be an image with the format we selected and Depth Attachment usage flag
		VkImageCreateInfo dimg_info = VkInit::ImageCreateInfo(m_DepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

		//for the depth image, we want to allocate it from GPU local memory
		VmaAllocationCreateInfo dimg_allocinfo = {};
		dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		//allocate and create the image
		vmaCreateImage(m_Allocator, &dimg_info, &dimg_allocinfo, &m_DepthImage.Image, &m_DepthImage.Allocation, nullptr);

		//build an image-view for the depth image to use for rendering
		VkImageViewCreateInfo dview_info = VkInit::ImageViewCreateInfo(m_DepthFormat, m_DepthImage.Image, VK_IMAGE_ASPECT_DEPTH_BIT);

		result = vkCreateImageView(m_Device, &dview_info, nullptr, &m_DepthImageView);
		assert(result == VK_SUCCESS);
	}

	void VulkanEngine::CreateImageViews()
	{
		m_SwapChainImageViews.resize(m_SwapChainImages.size());

		for (size_t i = 0; i < m_SwapChainImages.size(); i++)
		{
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_SwapChainImages[i];

			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_SwapChainImageFormat;

			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VkResult result = vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapChainImageViews[i]);
			assert(result == VK_SUCCESS);
		}
	}

	void VulkanEngine::CreateCommands()
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

		//create a command pool for commands submitted to the graphics queue.
		//we also want the pool to allow for resetting of individual command buffers
		VkCommandPoolCreateInfo commandPoolInfo = VkInit::CommandPoolCreateInfo(indices.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			VkResult result = vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_Frames[i].m_CommandPool);
			assert(result == VK_SUCCESS);

			//allocate the default command buffer that we will use for rendering
			VkCommandBufferAllocateInfo cmdAllocInfo = VkInit::CommadBufferAllocateInfo(m_Frames[i].m_CommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

			result = vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_Frames[i].m_MainCommandBuffer);
			assert(result == VK_SUCCESS);

			m_MainDeletionQueue.push_function([=]() 
			{
				vkDestroyCommandPool(m_Device, m_Frames[i].m_CommandPool, nullptr);
			});
		}

		VkCommandPoolCreateInfo uploadCommandPoolInfo = VkInit::CommandPoolCreateInfo(indices.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		//create pool for upload context
		VkResult result = vkCreateCommandPool(m_Device, &uploadCommandPoolInfo, nullptr, &m_UploadContext.m_CommandPool);
		assert(result == VK_SUCCESS);

		m_MainDeletionQueue.push_function([=]() 
		{
			vkDestroyCommandPool(m_Device, m_UploadContext.m_CommandPool, nullptr);
		});

		//allocate the default command buffer that we will use for the instant commands
		VkCommandBufferAllocateInfo cmdAllocInfo = VkInit::CommadBufferAllocateInfo(m_UploadContext.m_CommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		VkCommandBuffer cmd;
		result = vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_UploadContext.m_CommandBuffer);
		assert(result == VK_SUCCESS);
	}

	void VulkanEngine::CreateRenderPass()
	{
		// the renderpass will use this color attachment.
		VkAttachmentDescription colorAttachment = {};
		//the attachment will have the format needed by the swapchain
		colorAttachment.format = m_SwapChainImageFormat;
		//1 sample, we won't be doing MSAA
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		// we Clear when this attachment is loaded
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// we keep the attachment stored when the renderpass ends
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		//we don't care about stencil
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		//we don't know or care about the starting layout of the attachment
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		//after the renderpass ends, the image has to be on a layout ready for display
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		//attachment number will index into the pAttachments array in the parent renderpass itself
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment = {};
		// Depth attachment
		depthAttachment.flags = 0;
		depthAttachment.format = m_DepthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//we are going to create 1 subpass, which is the minimum you can do
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		//hook the depth attachment into the subpass
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		//array of 2 attachments, one for the color, and other for depth
		VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

		//2 attachments from said array
		renderPassInfo.attachmentCount = 2;
		renderPassInfo.pAttachments = &attachments[0];
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkSubpassDependency depthDependency = {};
		depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		depthDependency.dstSubpass = 0;
		depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.srcAccessMask = 0;
		depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkSubpassDependency dependencies[2] = { dependency, depthDependency };
				
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = &dependencies[0];

		VkResult result = vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass);
		assert(result == VK_SUCCESS);
	}

	void VulkanEngine::CreateFrameBuffers()
	{
		// Create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering.
		VkFramebufferCreateInfo frameBufferInfo = {};
		frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferInfo.pNext = nullptr;

		frameBufferInfo.renderPass = m_RenderPass;
		frameBufferInfo.attachmentCount = 1;
		frameBufferInfo.width = m_SwapChainExtent.width;
		frameBufferInfo.height = m_SwapChainExtent.height;
		frameBufferInfo.layers = 1;

		// Grab how many images we have in the swapchain.
		const uint32_t swapChainImageCount = m_SwapChainImages.size();
		m_FrameBuffers = std::vector<VkFramebuffer>(swapChainImageCount);

		// Create framebuffers for each of the swapchain image views.
		for (int i = 0; i < swapChainImageCount; i++) 
		{
			VkImageView attachments[2];
			attachments[0] = m_SwapChainImageViews[i];
			attachments[1] = m_DepthImageView;

			frameBufferInfo.pAttachments = attachments;
			frameBufferInfo.attachmentCount = 2;

			VkResult result = vkCreateFramebuffer(m_Device, &frameBufferInfo, nullptr, &m_FrameBuffers[i]);
			assert(result == VK_SUCCESS);
		}
	}

	void VulkanEngine::CreateSyncStructures()
	{
		// Create synchronization structures.
		VkFenceCreateInfo fenceCreateInfo = VkInit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

		// For the semaphores we don't need any flags.
		VkSemaphoreCreateInfo semaphoreCreateInfo = VkInit::SemaphoreCreateInfo(0);

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			VkResult result = vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_Frames[i].m_RenderFence);
			assert(result == VK_SUCCESS);

			//enqueue the destruction of the fence
			m_MainDeletionQueue.push_function([=]() 
			{
				vkDestroyFence(m_Device, m_Frames[i].m_RenderFence, nullptr);
			});

			result = vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].m_PresentSemaphore);
			assert(result == VK_SUCCESS);

			result = vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].m_RenderSemaphore);
			assert(result == VK_SUCCESS);

			//enqueue the destruction of semaphores
			m_MainDeletionQueue.push_function([=]() 
			{
				vkDestroySemaphore(m_Device, m_Frames[i].m_PresentSemaphore, nullptr);
				vkDestroySemaphore(m_Device, m_Frames[i].m_RenderSemaphore, nullptr);
			});
		}

		VkFenceCreateInfo uploadFenceCreateInfo = VkInit::FenceCreateInfo();

		VkResult result = vkCreateFence(m_Device, &uploadFenceCreateInfo, nullptr, &m_UploadContext.m_UploadFence);
		assert(result == VK_SUCCESS);

		m_MainDeletionQueue.push_function([=]() 
		{
			vkDestroyFence(m_Device, m_UploadContext.m_UploadFence, nullptr);
		});
	}

	void VulkanEngine::CreateGraphicsPipeline()
	{
		VkShaderModule triangleFragShader;
		if (!VkUtils::LoadShaderModule("res/shaders/triangle.frag.spv", m_Device, &triangleFragShader))
		{
			std::cout << "Error when building the triangle fragment shader module." << std::endl;
		}
		else 
		{
			std::cout << "Triangle fragment shader successfully loaded." << std::endl;
		}

		VkShaderModule triangleVertexShader;
		if (!VkUtils::LoadShaderModule("res/shaders/triangle.vert.spv", m_Device, &triangleVertexShader))
		{
			std::cout << "Error when building the triangle vertex shader module." << std::endl;
		}
		else 
		{
			std::cout << "Triangle vertex shader successfully loaded." << std::endl;
		}

		//build the pipeline layout that controls the inputs/outputs of the shader
		//we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::PipelineLayoutCreateInfo();

		VkResult result = vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_TrianglePipelineLayout);
		assert(result == VK_SUCCESS);

		//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
		PipelineBuilder pipelineBuilder;

		pipelineBuilder.m_ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));

		pipelineBuilder.m_ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

		//vertex input controls how to read vertices from vertex buffers. We aren't using it yet
		pipelineBuilder.m_VertexInputInfo = VkInit::VertexInputStateCreateInfo();

		//input assembly is the configuration for drawing triangle lists, strips, or individual points.
		//we are just going to draw triangle list
		pipelineBuilder.m_InputAssembly = VkInit::InputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		//build viewport and scissor from the swapchain extents
		pipelineBuilder.m_Viewport.x = 0.0f;
		pipelineBuilder.m_Viewport.y = 0.0f;
		pipelineBuilder.m_Viewport.width = (float)m_SwapChainExtent.width;
		pipelineBuilder.m_Viewport.height = (float)m_SwapChainExtent.height;
		pipelineBuilder.m_Viewport.minDepth = 0.0f;
		pipelineBuilder.m_Viewport.maxDepth = 1.0f;

		pipelineBuilder.m_Scissor.offset = { 0, 0 };
		pipelineBuilder.m_Scissor.extent = m_SwapChainExtent;

		//configure the rasterizer to draw filled triangles
		pipelineBuilder.m_Rasterizer = VkInit::RasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

		//we don't use multisampling, so just run the default one
		pipelineBuilder.m_Multisampling = VkInit::MultisamplingStateCreateInfo();

		//a single blend attachment with no blending and writing to RGBA
		pipelineBuilder.m_ColorBlendAttachment = VkInit::ColorBlendAttachmentState();

		//use the triangle layout we created
		pipelineBuilder.m_PipelineLayout = m_TrianglePipelineLayout;

		pipelineBuilder.m_DepthStencil = VkInit::DepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

		//finally build the pipeline
		m_TrianglePipeline = pipelineBuilder.BuildPipeline(m_Device, m_RenderPass);

		//build the mesh pipeline
		VertexInputDescription vertexDescription = Vertex::GetVertexDescription();

		//connect the pipeline builder vertex input info to the one we get from Vertex
		pipelineBuilder.m_VertexInputInfo.pVertexAttributeDescriptions = vertexDescription.Attributes.data();
		pipelineBuilder.m_VertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.Attributes.size();

		pipelineBuilder.m_VertexInputInfo.pVertexBindingDescriptions = vertexDescription.Bindings.data();
		pipelineBuilder.m_VertexInputInfo.vertexBindingDescriptionCount = vertexDescription.Bindings.size();

		//clear the shader stages for the builder
		pipelineBuilder.m_ShaderStages.clear();

		//compile mesh vertex shader
		VkShaderModule meshVertShader;
		if (!VkUtils::LoadShaderModule("res/shaders/triangleMesh.vert.spv", m_Device, &meshVertShader))
		{
			std::cout << "Error when building the triangle vertex shader module" << std::endl;
		}
		else {
			std::cout << "Red Triangle vertex shader successfully loaded" << std::endl;
		}

		//compile mesh fragment shader
		VkShaderModule meshFragShader;
		if (!VkUtils::LoadShaderModule("res/shaders/default_lit.frag.spv", m_Device, &meshFragShader))
		{
			std::cout << "Error when building the triangle fragment shader module" << std::endl;
		}
		else {
			std::cout << "Red Triangle fragment shader successfully loaded" << std::endl;
		}

		//compile mesh texured fragment shader
		VkShaderModule texMeshFragShader;
		if (!VkUtils::LoadShaderModule("res/shaders/textured_lit.frag.spv", m_Device, &texMeshFragShader))
		{
			std::cout << "Error when building the triangle fragment shader module" << std::endl;
		}
		else {
			std::cout << "Red Triangle fragment shader successfully loaded" << std::endl;
		}

		//add the other shaders
		pipelineBuilder.m_ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));

		//make sure that triangleFragShader is holding the compiled colored_triangle.frag
		pipelineBuilder.m_ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, meshFragShader));

		//we start from just the default empty pipeline layout info
		VkPipelineLayoutCreateInfo meshPipelineLayoutInfo = VkInit::PipelineLayoutCreateInfo();

		//setup push constants
		VkPushConstantRange pushConstant;
		//this push constant range starts at the beginning
		pushConstant.offset = 0;
		//this push constant range takes up the size of a MeshPushConstants struct
		pushConstant.size = sizeof(MeshPushConstants);
		//this push constant range is accessible only in the vertex shader
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		//push-constant setup
		meshPipelineLayoutInfo.pPushConstantRanges = &pushConstant;
		meshPipelineLayoutInfo.pushConstantRangeCount = 1;

		//hook the global set layout
		meshPipelineLayoutInfo.setLayoutCount = 1;
		meshPipelineLayoutInfo.pSetLayouts = &m_GlobalSetLayout;

		result = vkCreatePipelineLayout(m_Device, &meshPipelineLayoutInfo, nullptr, &m_MeshPipelineLayout);
		assert(result == VK_SUCCESS);

		pipelineBuilder.m_PipelineLayout = m_MeshPipelineLayout;

		//build the mesh triangle pipeline
		m_MeshPipeline = pipelineBuilder.BuildPipeline(m_Device, m_RenderPass);

		CreateMaterial(m_MeshPipeline, m_MeshPipelineLayout, "defaultMesh");

		//create pipeline layout for the textured mesh, which has 3 descriptor sets
		//we start from  the normal mesh layout
		VkPipelineLayoutCreateInfo textured_pipeline_layout_info = meshPipelineLayoutInfo;

		VkDescriptorSetLayout texturedSetLayouts[] = { m_GlobalSetLayout, m_SingleTextureSetLayout };

		textured_pipeline_layout_info.setLayoutCount = 2;
		textured_pipeline_layout_info.pSetLayouts = texturedSetLayouts;

		VkPipelineLayout texturedPipeLayout;
		result = vkCreatePipelineLayout(m_Device, &textured_pipeline_layout_info, nullptr, &texturedPipeLayout);
		assert(result == VK_SUCCESS);

		pipelineBuilder.m_ShaderStages.clear();

		//add the other shaders
		pipelineBuilder.m_ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));

		//make sure that triangleFragShader is holding the compiled colored_triangle.frag
		pipelineBuilder.m_ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, texMeshFragShader));

		pipelineBuilder.m_PipelineLayout = texturedPipeLayout;

		//build the mesh triangle pipeline
		m_TexturedMeshPipeline = pipelineBuilder.BuildPipeline(m_Device, m_RenderPass);

		CreateMaterial(m_TexturedMeshPipeline, texturedPipeLayout, "texturedMesh");

		//deleting all of the vulkan shaders
		vkDestroyShaderModule(m_Device, meshVertShader, nullptr);
		vkDestroyShaderModule(m_Device, meshFragShader, nullptr);
		vkDestroyShaderModule(m_Device, texMeshFragShader, nullptr);
		vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
		vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);

		m_MainDeletionQueue.push_function([=]()
		{
			vkDestroyPipeline(m_Device, m_TexturedMeshPipeline, nullptr);
			vkDestroyPipelineLayout(m_Device, texturedPipeLayout, nullptr);
		});
	}

	void VulkanEngine::CreateAllocator()
	{
		//initialize the memory allocator
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = m_PhysicalDevice;
		allocatorInfo.device = m_Device;
		allocatorInfo.instance = m_Instance;
		vmaCreateAllocator(&allocatorInfo, &m_Allocator);
	}

	AllocatedBuffer VulkanEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
	{
		//allocate vertex buffer
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;

		bufferInfo.size = allocSize;
		bufferInfo.usage = usage;

		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = memoryUsage;

		AllocatedBuffer newBuffer;

		//allocate the buffer
		VkResult res = vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaallocInfo, &newBuffer.Buffer, &newBuffer.Allocation, nullptr);
		assert(res == VK_SUCCESS);

		return newBuffer;
	}

	void VulkanEngine::CreateDescriptors()
	{
		const size_t sceneParamBufferSize = FRAME_OVERLAP * PadUniformBufferSize(sizeof(GPUSceneData));
		m_SceneParameterBuffer = CreateBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		//create a descriptor pool that will hold 10 uniform buffers
		std::vector<VkDescriptorPoolSize> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 }
		};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = 0;
		pool_info.maxSets = 10;
		pool_info.poolSizeCount = (uint32_t)sizes.size();
		pool_info.pPoolSizes = sizes.data();

		vkCreateDescriptorPool(m_Device, &pool_info, nullptr, &m_DescriptorPool);

		//binding for camera data at 0
		VkDescriptorSetLayoutBinding cameraBind = VkInit::DescriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

		//binding for scene data at 1
		VkDescriptorSetLayoutBinding sceneBind = VkInit::DescriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

		VkDescriptorSetLayoutBinding bindings[] = { cameraBind, sceneBind };

		VkDescriptorSetLayoutCreateInfo setinfo = {};
		setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setinfo.pNext = nullptr;
		//we are going to have 1 binding
		setinfo.bindingCount = 2;
		//no flags
		setinfo.flags = 0;
		//point to the camera buffer binding
		setinfo.pBindings = bindings;

		vkCreateDescriptorSetLayout(m_Device, &setinfo, nullptr, &m_GlobalSetLayout);

		//another set, one that holds a single texture
		VkDescriptorSetLayoutBinding textureBind = VkInit::DescriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

		VkDescriptorSetLayoutCreateInfo set3info = {};
		set3info.bindingCount = 1;
		set3info.flags = 0;
		set3info.pNext = nullptr;
		set3info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set3info.pBindings = &textureBind;

		vkCreateDescriptorSetLayout(m_Device, &set3info, nullptr, &m_SingleTextureSetLayout);

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			m_Frames[i].cameraBuffer = CreateBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			//allocate one descriptor set for each frame
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.pNext = nullptr;
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			//using the pool we just set
			allocInfo.descriptorPool = m_DescriptorPool;
			//only 1 descriptor
			allocInfo.descriptorSetCount = 1;
			//using the global data layout
			allocInfo.pSetLayouts = &m_GlobalSetLayout;

			vkAllocateDescriptorSets(m_Device, &allocInfo, &m_Frames[i].globalDescriptor);

			//information about the buffer we want to point at in the descriptor
			VkDescriptorBufferInfo cameraInfo;
			//it will be the camera buffer
			cameraInfo.buffer = m_Frames[i].cameraBuffer.Buffer;
			//at 0 offset
			cameraInfo.offset = 0;
			//of the size of a camera data struct
			cameraInfo.range = sizeof(GPUCameraData);

			VkDescriptorBufferInfo sceneInfo;
			sceneInfo.buffer = m_SceneParameterBuffer.Buffer;
			sceneInfo.offset = 0;
			sceneInfo.range = sizeof(GPUSceneData);

			VkWriteDescriptorSet cameraWrite = VkInit::WritDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_Frames[i].globalDescriptor, &cameraInfo, 0);

			VkWriteDescriptorSet sceneWrite = VkInit::WritDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, m_Frames[i].globalDescriptor, &sceneInfo, 1);

			VkWriteDescriptorSet setWrites[] = { cameraWrite, sceneWrite };

			vkUpdateDescriptorSets(m_Device, 2, setWrites, 0, nullptr);
		}

		// add buffers to deletion queues
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			m_MainDeletionQueue.push_function([&, i]()
			{
				vmaDestroyBuffer(m_Allocator, m_Frames[i].cameraBuffer.Buffer, m_Frames[i].cameraBuffer.Allocation);
			});
		}

		m_MainDeletionQueue.push_function([&]()
		{
			vmaDestroyBuffer(m_Allocator, m_SceneParameterBuffer.Buffer, m_SceneParameterBuffer.Allocation);
		});

		// add descriptor set layout to deletion queues
		m_MainDeletionQueue.push_function([&]() 
		{
			vkDestroyDescriptorSetLayout(m_Device, m_SingleTextureSetLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_Device, m_GlobalSetLayout, nullptr);
			vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
		});
	}

	// ------------------------------------------- HELPER ---------------------------------------

	void VulkanEngine::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	std::vector<const char*> VulkanEngine::GetRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (m_EnableValidationLayers) 
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		if (!CheckInstanceExtensionSupport(extensions))
			assert(false);

		return extensions;
	}

	bool VulkanEngine::CheckValidationLayerSupport()
	{
		// Validation layers.
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : m_ValidationLayers) 
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) 
			{
				if (strcmp(layerName, layerProperties.layerName) == 0) 
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound) 
			{
				return false;
			}
		}
	}

	bool VulkanEngine::CheckInstanceExtensionSupport(const std::vector<const char*>& glfwExtensionNames)
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		for (const char* extensionName : glfwExtensionNames)
		{
			bool extensionFound = false;

			for (const auto& extensionProperties : extensions)
			{
				if (strcmp(extensionName, extensionProperties.extensionName) == 0)
				{
					extensionFound = true;
					break;
				}
			}

			if (!extensionFound) {
				return false;
			}
		}
	}

	VkResult VulkanEngine::CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void VulkanEngine::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
	}

	size_t VulkanEngine::PadUniformBufferSize(size_t originalSize)
	{
		// Calculate required alignment based on minimum device offset alignment
		size_t minUboAlignment = m_GpuProperties.limits.minUniformBufferOffsetAlignment;
		size_t alignedSize = originalSize;
		if (minUboAlignment > 0) {
			alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}
		return alignedSize;
	}

	bool VulkanEngine::IsDeviceSuitable(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices = FindQueueFamilies(device);

		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) 
		{
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	QueueFamilyIndices VulkanEngine::FindQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}

			i++;
		}

		return indices;
	}

	VkPresentModeKHR VulkanEngine::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		for (const auto& availablePresentMode : availablePresentModes) 
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) 
			{
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanEngine::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
		{
			return capabilities.currentExtent;
		}
		else 
		{
			int width, height;
			glfwGetFramebufferSize(m_Window, &width, &height);

			VkExtent2D actualExtent = 
			{
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	VkSurfaceFormatKHR VulkanEngine::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats) 
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	SwapChainSupportDetails VulkanEngine::QuerySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;

		// Formats set up.
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

		if (formatCount != 0) 
		{
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.Formats.data());
		}

		// Present mode set up.
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) 
		{
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.PresentModes.data());
		}

		// Capabilities set up.
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.Capabilities);

		return details;
	}

	bool VulkanEngine::CheckDeviceExtensionSupport(VkPhysicalDevice device) 
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());

		for (const auto& extension : availableExtensions) 
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	VkPipeline PipelineBuilder::BuildPipeline(VkDevice device, VkRenderPass pass)
	{
		//make viewport state from our stored viewport and scissor.
		//at the moment we won't support multiple viewports or scissors
		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.pNext = nullptr;

		viewportState.viewportCount = 1;
		viewportState.pViewports = &m_Viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &m_Scissor;

		//setup dummy color blending. We aren't using transparent objects yet
		//the blending is just "no blend", but we do write to the color attachment
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.pNext = nullptr;

		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &m_ColorBlendAttachment;

		//build the actual pipeline
		//we now use all of the info structs we have been writing into into this one to create the pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = nullptr;

		pipelineInfo.stageCount = m_ShaderStages.size();
		pipelineInfo.pStages = m_ShaderStages.data();
		pipelineInfo.pVertexInputState = &m_VertexInputInfo;
		pipelineInfo.pInputAssemblyState = &m_InputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &m_Rasterizer;
		pipelineInfo.pMultisampleState = &m_Multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = m_PipelineLayout;
		pipelineInfo.renderPass = pass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.pDepthStencilState = &m_DepthStencil;

		//it's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
		VkPipeline newPipeline;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) 
		{
			std::cout << "Failed to create pipeline.\n";
			return VK_NULL_HANDLE; // failed to create graphics pipeline
		}
		else
		{
			return newPipeline;
		}
	}

	void VulkanEngine::LoadMeshes()
	{
		m_TriangleMesh.Vertices.resize(3);

		// Vertex positions.
		m_TriangleMesh.Vertices[0].Position = { 1.f, 1.f, 0.f };
		m_TriangleMesh.Vertices[1].Position = {-1.f, 1.f, 0.f };
		m_TriangleMesh.Vertices[2].Position = { 0.f,-1.f, 0.f };

		// Vertex colors.
		m_TriangleMesh.Vertices[0].Color = { 0.f, 1.f, 0.f };
		m_TriangleMesh.Vertices[1].Color = { 0.f, 1.f, 0.f };
		m_TriangleMesh.Vertices[2].Color = { 0.f, 1.f, 0.f };

		//load the monkey
		m_MonkeyMesh.LoadFromObj("res/assets/monkey_smooth.obj");

		UploadMesh(m_TriangleMesh);
		UploadMesh(m_MonkeyMesh);

		m_Meshes["monkey"] = m_MonkeyMesh;
		m_Meshes["triangle"] = m_TriangleMesh;

		Mesh lostEmpire{};
		lostEmpire.LoadFromObj("res/assets/lost_empire.obj");

		UploadMesh(lostEmpire);

		m_Meshes["empire"] = lostEmpire;
	}

	void VulkanEngine::UploadMesh(Mesh& mesh)
	{
		const size_t bufferSize = mesh.Vertices.size() * sizeof(Vertex);

		//allocate staging buffer
		VkBufferCreateInfo stagingBufferInfo = {};
		stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBufferInfo.pNext = nullptr;
		stagingBufferInfo.size = bufferSize;
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		//let the VMA library know that this data should be on CPU RAM
		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		AllocatedBuffer stagingBuffer;

		//allocate the buffer
		VkResult result = vmaCreateBuffer(m_Allocator, &stagingBufferInfo, &vmaallocInfo, &stagingBuffer.Buffer, &stagingBuffer.Allocation, nullptr);
		assert(result == VK_SUCCESS);

		void* data;
		vmaMapMemory(m_Allocator, stagingBuffer.Allocation, &data);
		memcpy(data, mesh.Vertices.data(), mesh.Vertices.size() * sizeof(Vertex));
		vmaUnmapMemory(m_Allocator, stagingBuffer.Allocation);

		//allocate vertex buffer
		VkBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.pNext = nullptr;
		//this is the total size, in bytes, of the buffer we are allocating
		vertexBufferInfo.size = bufferSize;
		//this buffer is going to be used as a Vertex Buffer
		vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		//let the VMA library know that this data should be GPU native
		vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		//allocate the buffer
		result = vmaCreateBuffer(m_Allocator, &vertexBufferInfo, &vmaallocInfo, &mesh.VertexBuffer.Buffer, &mesh.VertexBuffer.Allocation, nullptr);
		assert(result == VK_SUCCESS);

		ImmediateSubmit([=](VkCommandBuffer cmd) 
		{
			VkBufferCopy copy;
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = bufferSize;
			vkCmdCopyBuffer(cmd, stagingBuffer.Buffer, mesh.VertexBuffer.Buffer, 1, &copy);
		});

		//add the destruction of mesh buffer to the deletion queue
		m_MainDeletionQueue.push_function([=]() 
		{
			vmaDestroyBuffer(m_Allocator, mesh.VertexBuffer.Buffer, mesh.VertexBuffer.Allocation);
		});

		vmaDestroyBuffer(m_Allocator, stagingBuffer.Buffer, stagingBuffer.Allocation);
	}

	void VulkanEngine::InitScene()
	{
		RenderObject monkey;
		monkey.mesh = GetMesh("monkey");
		monkey.material = GetMaterial("defaultMesh");
		monkey.transformMatrix = glm::mat4{ 1.0f };

		m_Renderables.push_back(monkey);

		//create a sampler for the texture
		VkSamplerCreateInfo samplerInfo = VkInit::SamplerCreateInfo(VK_FILTER_NEAREST);

		VkSampler blockySampler;
		vkCreateSampler(m_Device, &samplerInfo, nullptr, &blockySampler);

		RenderObject map;
		map.mesh = GetMesh("empire");
		map.material = GetMaterial("texturedMesh");
		map.transformMatrix = glm::translate(glm::vec3{ 5, -10, 0 });

		//allocate the descriptor set for single-texture to use on the material
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.pNext = nullptr;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_SingleTextureSetLayout;

		vkAllocateDescriptorSets(m_Device, &allocInfo, &map.material->textureSet);

		//write to the descriptor set so that it points to our empire_diffuse texture
		VkDescriptorImageInfo imageBufferInfo;
		imageBufferInfo.sampler = blockySampler;
		imageBufferInfo.imageView = m_LoadedTextures["empire_diffuse"].ImageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet texture1 = VkInit::WriteDescriptorImage(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, map.material->textureSet, &imageBufferInfo, 0);

		vkUpdateDescriptorSets(m_Device, 1, &texture1, 0, nullptr);

		m_Renderables.push_back(map);

		m_MainDeletionQueue.push_function([=]()
		{
			vkDestroySampler(m_Device, blockySampler, m_Allocator->GetAllocationCallbacks());
		});

		for (int x = -20; x <= 20; x++) {
			for (int y = -20; y <= 20; y++) {

				RenderObject triangle;
				triangle.mesh = GetMesh("triangle");
				triangle.material = GetMaterial("defaultMesh");
				glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
				glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
				triangle.transformMatrix = translation * scale;

				m_Renderables.push_back(triangle);
			}
		}
	}

	Material* VulkanEngine::CreateMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
	{
		Material mat;
		mat.pipeline = pipeline;
		mat.pipelineLayout = layout;
		m_Materials[name] = mat;
		return &m_Materials[name];
	}

	Material* VulkanEngine::GetMaterial(const std::string& name)
	{
		//search for the object, and return nullptr if not found
		auto it = m_Materials.find(name);
		if (it == m_Materials.end()) {
			return nullptr;
		}
		else {
			return &(*it).second;
		}
	}


	Mesh* VulkanEngine::GetMesh(const std::string& name)
	{
		auto it = m_Meshes.find(name);
		if (it == m_Meshes.end()) {
			return nullptr;
		}
		else {
			return &(*it).second;
		}
	}


	void VulkanEngine::DrawObjects(VkCommandBuffer cmd, RenderObject* first, int count)
	{
		//camera view
		glm::vec3 camPos = { 0.f,-6.f,-10.f };

		glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
		//camera projection
		glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
		projection[1][1] *= -1;

		//fill a GPU camera data struct
		GPUCameraData camData;
		camData.proj = projection;
		camData.view = view;
		camData.viewproj = projection * view;

		//and copy it to the buffer
		void* data;
		vmaMapMemory(m_Allocator, GetCurrentFrame().cameraBuffer.Allocation, &data);
		memcpy(data, &camData, sizeof(GPUCameraData));
		vmaUnmapMemory(m_Allocator, GetCurrentFrame().cameraBuffer.Allocation);

		float framed = (m_FrameNumber / 120.f);
		m_SceneParameters.ambientColor = { sin(framed), 0, cos(framed), 1 };
		char* sceneData;
		vmaMapMemory(m_Allocator, m_SceneParameterBuffer.Allocation, (void**)&sceneData);
		int frameIndex = m_FrameNumber % FRAME_OVERLAP;
		sceneData += PadUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;
		memcpy(sceneData, &m_SceneParameters, sizeof(GPUSceneData));
		vmaUnmapMemory(m_Allocator, m_SceneParameterBuffer.Allocation);

		Mesh* lastMesh = nullptr;
		Material* lastMaterial = nullptr;

		for (int i = 0; i < count; i++)
		{
			RenderObject& object = first[i];

			//only bind the pipeline if it doesn't match with the already bound one
			if (object.material != lastMaterial) 
			{
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
				lastMaterial = object.material;

				//offset for our scene buffer
				uint32_t uniformOffset = PadUniformBufferSize(sizeof(GPUSceneData)) * frameIndex;

				//bind the descriptor set when changing pipeline
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &GetCurrentFrame().globalDescriptor, 1, &uniformOffset);

				if (object.material->textureSet != VK_NULL_HANDLE) 
				{
					//texture descriptor
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &object.material->textureSet, 0, nullptr);
				}
			}

			MeshPushConstants constants;
			constants.renderMatrix = object.transformMatrix;

			//upload the mesh to the GPU via push constants
			vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

			//only bind the mesh if it's a different one from last bind
			if (object.mesh != lastMesh) 
			{
				//bind the mesh vertex buffer with offset 0
				VkDeviceSize offset = 0;
				vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->VertexBuffer.Buffer, &offset);
				lastMesh = object.mesh;
			}

			//we can now draw
			vkCmdDraw(cmd, object.mesh->Vertices.size(), 1, 0, 0);
		}
	}

	FrameData& VulkanEngine::GetCurrentFrame()
	{
		return m_Frames[m_FrameNumber % FRAME_OVERLAP];
	}

	void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
	{
		VkCommandBuffer cmd = m_UploadContext.m_CommandBuffer;

		//begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that.
		VkCommandBufferBeginInfo cmdBeginInfo = VkInit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		VkResult result = vkBeginCommandBuffer(cmd, &cmdBeginInfo);
		assert(result == VK_SUCCESS);

		// excecute function
		function(cmd);

		result = vkEndCommandBuffer(cmd);
		assert(result == VK_SUCCESS);

		VkSubmitInfo submit = VkInit::SubmitInfo(&cmd);

		//submit command buffer to the queue and execute it.
		// _uploadFence will now block until the graphic commands finish execution
		result = vkQueueSubmit(m_GraphicsQueue, 1, &submit, m_UploadContext.m_UploadFence);
		assert(result == VK_SUCCESS);

		vkWaitForFences(m_Device, 1, &m_UploadContext.m_UploadFence, true, 9999999999);
		vkResetFences(m_Device, 1, &m_UploadContext.m_UploadFence);

		// reset the command buffers inside the command pool
		vkResetCommandPool(m_Device, m_UploadContext.m_CommandPool, 0);
	}

	void VulkanEngine::LoadImages()
	{
		Texture lostEmpire;

		VkUtils::LoadImageFromFile(*this, "res/assets/lost_empire-RGBA.png", lostEmpire.Image);

		VkImageViewCreateInfo imageinfo = VkInit::ImageViewCreateInfo(VK_FORMAT_R8G8B8A8_SRGB, lostEmpire.Image.Image, VK_IMAGE_ASPECT_COLOR_BIT);
		vkCreateImageView(m_Device, &imageinfo, nullptr, &lostEmpire.ImageView);

		m_LoadedTextures["empire_diffuse"] = lostEmpire;
	}
}