#include "Application.h"

#include <cassert>

namespace tessera
{
	// TODO: One idea how to avoid these dependencies, is to create VulkanTransferObject and pass it in every init by reference.

	void Application::init()
	{
		glfwInitializer.init();
		window = glfwInitializer.getWindow();
		assert(window);

		vulkanInstanceManager.init();
		instance = vulkanInstanceManager.getInstance();
		assert(instance);

		debugManager.init(instance);

		surfaceManager.init(instance, window);
		surface = surfaceManager.getSurface();
		assert(surface);

		deviceManager.init(instance, surface); 
		swapChainManager.init(deviceManager, surface, window);
		imageViewManager.init(swapChainManager.getSwapChainImageDetails(), deviceManager.getLogicalDevice());

		graphicsPipelineManager.init(deviceManager.getLogicalDevice(), swapChainManager.getSwapChainImageDetails());
		framebufferManager.init(imageViewManager.getSwapChainImageViews(), deviceManager.getLogicalDevice(), graphicsPipelineManager.getRenderPath(), swapChainManager.getSwapChainImageDetails());
		clean();
	}

	void Application::clean() const
	{
		framebufferManager.clean(deviceManager.getLogicalDevice());
		graphicsPipelineManager.clean(deviceManager.getLogicalDevice());
		imageViewManager.clean(deviceManager.getLogicalDevice());
		swapChainManager.clean(deviceManager.getLogicalDevice());
		deviceManager.clean();
		debugManager.clean(instance);
		surfaceManager.clean(instance);
		vulkanInstanceManager.clean();
		glfwInitializer.clean();
	}

}
