#pragma once

#include <GLFW\glfw3.h>
#include <vma\vk_mem_alloc.h>

namespace VKE
{

	struct AllocatedBuffer
	{
		VkBuffer Buffer;
		VmaAllocation Allocation;
	};

	struct AllocatedImage 
	{
		VkImage Image;
		VmaAllocation Allocation;
	};

}
