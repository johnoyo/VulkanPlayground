#pragma once

#include "GLFW\glfw3.h"

#include <vector>
#include <fstream>

namespace VKE
{
	class VkUtils
	{
	public:
		// Loads a shader module from a spir-v file. Returns false if it errors
		static bool LoadShaderModule(const std::string& filePath, VkDevice device, VkShaderModule* outShaderModule);
	};
}