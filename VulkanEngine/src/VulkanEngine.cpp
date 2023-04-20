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

namespace VKE
{
	void VulkanEngine::Init()
	{
		InitWindow();
		InitVulkan();
		LoadMeshes();
		LoadBatch1();
		LoadBatch2();
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
		result = vkWaitForFences(m_Device, 1, &m_RenderFence, true, 1000000000);
		assert(result == VK_SUCCESS);

		result = vkResetFences(m_Device, 1, &m_RenderFence);
		assert(result == VK_SUCCESS);

		uint32_t swapchainImageIndex;
		result = vkAcquireNextImageKHR(m_Device, m_SwapChain, 1000000000, m_PresentSemaphore, nullptr, &swapchainImageIndex);
		assert(result == VK_SUCCESS);

		// Now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
		result = vkResetCommandBuffer(m_MainCommandBuffer, 0);
		assert(result == VK_SUCCESS);

		VkCommandBuffer cmd = m_MainCommandBuffer;

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
		float flash = std::abs(std::sin(m_FrameNumber / 120.f));
		clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

		// Start the main renderpass.
		// We will use the clear color from above, and the framebuffer of the index the swapchain gave us
		VkRenderPassBeginInfo rpInfo = {};
		rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpInfo.pNext = nullptr;

		rpInfo.renderPass = m_RenderPass;
		rpInfo.renderArea.offset.x = 0;
		rpInfo.renderArea.offset.y = 0;
		rpInfo.renderArea.extent = m_SwapChainExtent;
		rpInfo.framebuffer = m_FrameBuffers[swapchainImageIndex];

		//connect clear values
		rpInfo.clearValueCount = 1;
		rpInfo.pClearValues = &clearValue;

		vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

		//once we start adding rendering commands, they will go here

		// Hardcoded Triangle pipeline.
		//vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_TrianglePipeline);
		//vkCmdDraw(cmd, 3, 1, 0, 0);

		// Triagle mesh pipeline.
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MeshPipeline);

		////bind the mesh vertex buffer with offset 0
		//VkDeviceSize offset = 0;
		//vkCmdBindVertexBuffers(cmd, 0, 1, &m_TriangleMesh.VertexBuffer.Buffer, &offset);

		////we can now draw the mesh
		//vkCmdDraw(cmd, m_TriangleMesh.Vertices.size(), 1, 0, 0);

		//bind the mesh vertex buffer with offset 0
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &m_QuadsBatch1.VertexBuffer.Buffer, &offset);

		vkCmdBindIndexBuffer(cmd, m_QuadsBatch1.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(cmd, m_QuadsBatch1.Indeces.size(), 1, 0, 0, 0);

		offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &m_QuadsBatch2.VertexBuffer.Buffer, &offset);

		vkCmdBindIndexBuffer(cmd, m_QuadsBatch2.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(cmd, m_QuadsBatch2.Indeces.size(), 1, 0, 0, 0);

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
		submit.pWaitSemaphores = &m_PresentSemaphore;

		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &m_RenderSemaphore;

		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cmd;

		//submit command buffer to the queue and execute it.
		// _renderFence will now block until the graphic commands finish execution
		result = vkQueueSubmit(m_GraphicsQueue, 1, &submit, m_RenderFence);
		assert(result == VK_SUCCESS);

		// this will put the image we just rendered into the visible window.
		// we want to wait on the _renderSemaphore for that,
		// as it's necessary that drawing commands have finished before the image is displayed to the user
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;

		presentInfo.pSwapchains = &m_SwapChain;
		presentInfo.swapchainCount = 1;

		presentInfo.pWaitSemaphores = &m_RenderSemaphore;
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

		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

		vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);

		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

		vkDestroyPipelineLayout(m_Device, m_TrianglePipelineLayout, nullptr);

		vkDestroyPipeline(m_Device, m_TrianglePipeline, nullptr);

		vkDestroyPipeline(m_Device, m_MeshPipeline, nullptr);

		for (int i = 0; i < m_FrameBuffers.size(); i++) 
		{
			vkDestroyFramebuffer(m_Device, m_FrameBuffers[i], nullptr);
			vkDestroyImageView(m_Device, m_SwapChainImageViews[i], nullptr);
		}

		vkDestroyFence(m_Device, m_RenderFence, nullptr);

		vkDestroySemaphore(m_Device, m_PresentSemaphore, nullptr);
		vkDestroySemaphore(m_Device, m_RenderSemaphore, nullptr);

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
		CreateSwapChain();
		CreateImageViews();
		CreateCommands();
		CreateRenderPass();
		CreateFrameBuffers();
		CreateSyncStructures();
		CreateGraphicsPipeline();
		CreateAllocator();
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
		//create a command pool for commands submitted to the graphics queue.
		VkCommandPoolCreateInfo commandPoolInfo = {};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.pNext = nullptr;

		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

		//the command pool will be one that can submit graphics commands
		commandPoolInfo.queueFamilyIndex = indices.graphicsFamily.value();
		//we also want the pool to allow for resetting of individual command buffers
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VkResult result = vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_CommandPool);
		assert(result == VK_SUCCESS);

		//allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = {};
		cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo.pNext = nullptr;

		//commands will be made from our _commandPool
		cmdAllocInfo.commandPool = m_CommandPool;
		//we will allocate 1 command buffer
		cmdAllocInfo.commandBufferCount = 1;
		// command level is Primary
		cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		result = vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_MainCommandBuffer);
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

		//we are going to create 1 subpass, which is the minimum you can do
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

		//connect the color attachment to the info
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		//connect the subpass to the info
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

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
			frameBufferInfo.pAttachments = &m_SwapChainImageViews[i];
			VkResult result = vkCreateFramebuffer(m_Device, &frameBufferInfo, nullptr, &m_FrameBuffers[i]);
			assert(result == VK_SUCCESS);
		}
	}

	void VulkanEngine::CreateSyncStructures()
	{
		// Create synchronization structures.
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.pNext = nullptr;

		// We want to create the fence with the Create Signaled flag, so we can wait on it before using it on a GPU command (for the first frame).
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		VkResult result = vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_RenderFence);
		assert(result == VK_SUCCESS);

		// For the semaphores we don't need any flags.
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		result = vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_PresentSemaphore);
		assert(result == VK_SUCCESS);

		result = vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_RenderSemaphore);
		assert(result == VK_SUCCESS);
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

		//compile mesh vertex shader
		VkShaderModule meshFragShader;
		if (!VkUtils::LoadShaderModule("res/shaders/triangleMesh.frag.spv", m_Device, &meshFragShader))
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

		//build the mesh triangle pipeline
		m_MeshPipeline = pipelineBuilder.BuildPipeline(m_Device, m_RenderPass);

		//deleting all of the vulkan shaders
		vkDestroyShaderModule(m_Device, meshVertShader, nullptr);
		vkDestroyShaderModule(m_Device, meshFragShader, nullptr);
		vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
		vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);
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

	void VulkanEngine::LoadBatch1()
	{
		m_QuadsBatch1.Vertices.resize(8);

		// Vertex positions.
		m_QuadsBatch1.Vertices[0].Position = { 0.8f, 0.8f, 0.f };
		m_QuadsBatch1.Vertices[1].Position = {-0.8f, 0.8f, 0.f };
		m_QuadsBatch1.Vertices[2].Position = {-0.8f,-0.8f, 0.f };
		m_QuadsBatch1.Vertices[3].Position = { 0.8f,-0.8f, 0.f };

		m_QuadsBatch1.Vertices[4].Position = { 0.6f, 0.6f, 0.f };
		m_QuadsBatch1.Vertices[5].Position = {-0.6f, 0.6f, 0.f };
		m_QuadsBatch1.Vertices[6].Position = {-0.6f,-0.6f, 0.f };
		m_QuadsBatch1.Vertices[7].Position = { 0.6f,-0.6f, 0.f };

		// Vertex colors.
		m_QuadsBatch1.Vertices[0].Color = { 1.f, 1.f, 0.f };
		m_QuadsBatch1.Vertices[1].Color = { 1.f, 1.f, 0.f };
		m_QuadsBatch1.Vertices[2].Color = { 1.f, 1.f, 0.f };
		m_QuadsBatch1.Vertices[3].Color = { 1.f, 1.f, 0.f };

		m_QuadsBatch1.Vertices[4].Color = { 1.f, 0.f, 0.f };
		m_QuadsBatch1.Vertices[5].Color = { 1.f, 0.f, 0.f };
		m_QuadsBatch1.Vertices[6].Color = { 1.f, 0.f, 0.f };
		m_QuadsBatch1.Vertices[7].Color = { 1.f, 0.f, 0.f };

		m_QuadsBatch1.Indeces.push_back(0);
		m_QuadsBatch1.Indeces.push_back(1);
		m_QuadsBatch1.Indeces.push_back(2);

		m_QuadsBatch1.Indeces.push_back(0);
		m_QuadsBatch1.Indeces.push_back(2);
		m_QuadsBatch1.Indeces.push_back(3);

		m_QuadsBatch1.Indeces.push_back(4);
		m_QuadsBatch1.Indeces.push_back(5);
		m_QuadsBatch1.Indeces.push_back(6);

		m_QuadsBatch1.Indeces.push_back(4);
		m_QuadsBatch1.Indeces.push_back(6);
		m_QuadsBatch1.Indeces.push_back(7);

		UploadQuads(m_QuadsBatch1);
	}

	void VulkanEngine::LoadBatch2()
	{
		m_QuadsBatch2.Vertices.resize(8);

		// Vertex positions.
		m_QuadsBatch2.Vertices[0].Position = { 0.4f, 0.4f, 0.f };
		m_QuadsBatch2.Vertices[1].Position = {-0.4f, 0.4f, 0.f };
		m_QuadsBatch2.Vertices[2].Position = {-0.4f,-0.4f, 0.f };
		m_QuadsBatch2.Vertices[3].Position = { 0.4f,-0.4f, 0.f };
					
		m_QuadsBatch2.Vertices[4].Position = { 0.2f, 0.2f, 0.f };
		m_QuadsBatch2.Vertices[5].Position = {-0.2f, 0.2f, 0.f };
		m_QuadsBatch2.Vertices[6].Position = {-0.2f,-0.2f, 0.f };
		m_QuadsBatch2.Vertices[7].Position = { 0.2f,-0.2f, 0.f };

		// Vertex colors.
		m_QuadsBatch2.Vertices[0].Color = { 0.f, 0.f, 1.f };
		m_QuadsBatch2.Vertices[1].Color = { 0.f, 0.f, 1.f };
		m_QuadsBatch2.Vertices[2].Color = { 0.f, 0.f, 1.f };
		m_QuadsBatch2.Vertices[3].Color = { 0.f, 0.f, 1.f };
					
		m_QuadsBatch2.Vertices[4].Color = { 1.f, 0.f, 1.f };
		m_QuadsBatch2.Vertices[5].Color = { 1.f, 0.f, 1.f };
		m_QuadsBatch2.Vertices[6].Color = { 1.f, 0.f, 1.f };
		m_QuadsBatch2.Vertices[7].Color = { 1.f, 0.f, 1.f };
					
		m_QuadsBatch2.Indeces.push_back(0);
		m_QuadsBatch2.Indeces.push_back(1);
		m_QuadsBatch2.Indeces.push_back(2);
					
		m_QuadsBatch2.Indeces.push_back(0);
		m_QuadsBatch2.Indeces.push_back(2);
		m_QuadsBatch2.Indeces.push_back(3);
					
		m_QuadsBatch2.Indeces.push_back(4);
		m_QuadsBatch2.Indeces.push_back(5);
		m_QuadsBatch2.Indeces.push_back(6);
					
		m_QuadsBatch2.Indeces.push_back(4);
		m_QuadsBatch2.Indeces.push_back(6);
		m_QuadsBatch2.Indeces.push_back(7);

		UploadQuads(m_QuadsBatch2);
	}

	void VulkanEngine::UploadQuads(Mesh& mesh)
	{
		// Allocate vertex buffer.
		VkBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = mesh.Vertices.size() * sizeof(Vertex);
		vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		VmaAllocationCreateInfo vmaAllocationInfo = {};
		vmaAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		// Allocate buffer.
		VkResult result = vmaCreateBuffer(m_Allocator, &vertexBufferInfo, &vmaAllocationInfo, &mesh.VertexBuffer.Buffer, &mesh.VertexBuffer.Allocation, nullptr);
		assert(result == VK_SUCCESS);

		void* vertexData;
		vmaMapMemory(m_Allocator, mesh.VertexBuffer.Allocation, &vertexData);
		memcpy(vertexData, mesh.Vertices.data(), mesh.Vertices.size() * sizeof(Vertex));
		vmaUnmapMemory(m_Allocator, mesh.VertexBuffer.Allocation);

		// Allocate index buffer.
		VkBufferCreateInfo indexBufferInfo = {};
		indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		indexBufferInfo.size = mesh.Indeces.size() * sizeof(uint32_t);
		indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		vmaAllocationInfo = {};
		vmaAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		// Allocate buffer.
		result = vmaCreateBuffer(m_Allocator, &indexBufferInfo, &vmaAllocationInfo, &mesh.IndexBuffer.Buffer, &mesh.IndexBuffer.Allocation, nullptr);
		assert(result == VK_SUCCESS);

		void* indexData;
		vmaMapMemory(m_Allocator, mesh.IndexBuffer.Allocation, &indexData);
		memcpy(indexData, mesh.Indeces.data(), mesh.Indeces.size() * sizeof(uint32_t));
		vmaUnmapMemory(m_Allocator, mesh.IndexBuffer.Allocation);
	}

	void VulkanEngine::LoadMeshes()
	{
		m_TriangleMesh.Vertices.resize(3);

		// Vertex positions.
		m_TriangleMesh.Vertices[0].Position = { 1.f, 1.f, 0.f };
		m_TriangleMesh.Vertices[1].Position = {-1.f, 1.f, 0.f };
		m_TriangleMesh.Vertices[2].Position = { 0.f,-1.f, 0.f };

		// Vertex colors.
		m_TriangleMesh.Vertices[0].Color = { 1.f, 0.f, 0.f };
		m_TriangleMesh.Vertices[1].Color = { 0.f, 1.f, 0.f };
		m_TriangleMesh.Vertices[2].Color = { 0.f, 0.f, 1.f };

		UploadMesh(m_TriangleMesh);
	}

	void VulkanEngine::UploadMesh(Mesh& mesh)
	{
		// Allocate vertex buffer.
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = mesh.Vertices.size() * sizeof(Vertex);
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		VmaAllocationCreateInfo vmaAllocationInfo = {};
		vmaAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

		// Allocate buffer.
		VkResult result = vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaAllocationInfo, &mesh.VertexBuffer.Buffer, &mesh.VertexBuffer.Allocation, nullptr);
		assert(result == VK_SUCCESS);

		void* data;
		vmaMapMemory(m_Allocator, mesh.VertexBuffer.Allocation, &data);
		memcpy(data, mesh.Vertices.data(), mesh.Vertices.size() * sizeof(Vertex));
		vmaUnmapMemory(m_Allocator, mesh.VertexBuffer.Allocation);
	}
}