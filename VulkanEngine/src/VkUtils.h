#pragma once

#include "GLFW\glfw3.h"

#include "VkTypes.h"
#include "VulkanEngine.h"

#include <vector>
#include <fstream>

namespace VKE
{
	class VkUtils
	{
	public:
		// Loads a shader module from a spir-v file. Returns false if it errors.
		static bool LoadShaderModule(const std::string& filePath, VkDevice device, VkShaderModule* outShaderModule);

		static bool LoadImageFromFile(VulkanEngine& engine, const char* file, AllocatedImage& outImage);
	};
}