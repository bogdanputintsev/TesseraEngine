#include "ImageViewManager.h"

#include <stdexcept>

#include "utils/interfaces/ServiceLocator.h"

namespace tessera::vulkan
{

	void ImageViewManager::init()
	{
		const auto& device = ServiceLocator::getService<DeviceManager>()->getLogicalDevice();
		const auto& [swapChainImageFormat, swapChainExtent, swapChainImages]
			= ServiceLocator::getService<SwapChainManager>()->getSwapChainImageDetails();

		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++) 
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) 
			{
				throw std::runtime_error("VulkanImageViewManager: failed to create image views.");
			}
		}
	}

	void ImageViewManager::clean()
	{
		const auto& device = ServiceLocator::getService<DeviceManager>()->getLogicalDevice();

		for (const auto& imageView : swapChainImageViews) 
		{
			vkDestroyImageView(device, imageView, nullptr);
		}
	}

}
