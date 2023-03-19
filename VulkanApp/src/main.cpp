#include "VulkanEngine.h"

int main()
{
	VKE::VulkanEngine* vkEngine = new VKE::VulkanEngine;

	vkEngine->Init();
	vkEngine->Run();
	vkEngine->Cleanup();

	delete vkEngine;

	return 0;
}