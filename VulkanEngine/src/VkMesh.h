#pragma once

#include "VkTypes.h"

#include <glm\vec3.hpp>
#include <glm\vec2.hpp>

#include <iostream>
#include <vector>

namespace VKE
{
	struct VertexInputDescription 
	{
		std::vector<VkVertexInputBindingDescription> Bindings;
		std::vector<VkVertexInputAttributeDescription> Attributes;

		VkPipelineVertexInputStateCreateFlags Flags = 0;
	};

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Color;
		glm::vec2 UV;

		static VertexInputDescription GetVertexDescription();
	};

	struct Mesh
	{
		std::vector<Vertex> Vertices;
		AllocatedBuffer VertexBuffer;

		bool LoadFromObj(const char* filename);
	};
}
