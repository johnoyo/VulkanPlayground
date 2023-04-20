#pragma once

#include "VkTypes.h"

#include <glm\vec3.hpp>
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

		static VertexInputDescription GetVertexDescription();
	};

	struct Mesh
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indeces;
		AllocatedBuffer VertexBuffer;
		AllocatedBuffer IndexBuffer;
	};
}
