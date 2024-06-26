#include "SwapChainManager.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <GLFW/glfw3.h>

#include "DeviceManager.h"
#include "FramebufferManager.h"
#include "ImageViewManager.h"
#include "QueueManager.h"
#include "SurfaceManager.h"
#include "SyncObjectsManager.h"
#include "glfw/GlfwInitializer.h"
#include "utils/interfaces/ServiceLocator.h"

namespace tessera::vulkan
{
	void SwapChainManager::init()
	{
		const auto& deviceManager = ServiceLocator::getService<DeviceManager>();
		const auto& window = ServiceLocator::getService<glfw::GlfwInitializer>()->getWindow();
		const auto& surface = ServiceLocator::getService<SurfaceManager>()->getSurface();

		const auto physicalDevice = deviceManager->getPhysicalDevice();
		assert(physicalDevice);
		const auto logicalDevice = deviceManager->getLogicalDevice();
		assert(logicalDevice);

		const auto [capabilities, formats, presentModes] = querySwapChainSupport(physicalDevice, surface);

		const auto [format, colorSpace] = chooseSwapSurfaceFormat(formats);
		const VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
		const VkExtent2D extent = chooseSwapExtent(capabilities, window);

		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) 
		{
			imageCount = capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = format;
		createInfo.imageColorSpace = colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		const auto [graphicsFamily, presentFamily] = findQueueFamilies(physicalDevice, surface);
		const uint32_t queueFamilyIndices[] = { graphicsFamily.value(), presentFamily.value() };

		if (graphicsFamily != presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("VulkanSwapChainManager: failed to create swap chain.");
		}

		std::vector<VkImage> swapChainImages;
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

		swapChainDetails = { format, extent, swapChainImages };
	}

	std::optional<uint32_t> SwapChainManager::acquireNextImage(const int currentFrame) const
	{
		const auto& device = ServiceLocator::getService<DeviceManager>()->getLogicalDevice();
		const auto& imageAvailableSemaphore = ServiceLocator::getService<SyncObjectsManager>()->getImageAvailableSemaphores();
		const auto& swapChainManager = ServiceLocator::getService<SwapChainManager>();

		if (static_cast<size_t>(currentFrame) >= imageAvailableSemaphore.size() || currentFrame < 0)
		{
			throw std::out_of_range("SyncObjectsManager: current frame number is larger than number of fences.");
		}

		uint32_t imageIndex;
		const VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) 
		{
			swapChainManager->recreate();
			return std::nullopt;
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) 
		{
			throw std::runtime_error("SyncObjectsManager: failed to acquire swap chain image.");
		}

		return imageIndex;
	}

	void SwapChainManager::recreate()
	{
		const auto& deviceManager = ServiceLocator::getService<DeviceManager>();
		const auto& imageViewManager = ServiceLocator::getService<ImageViewManager>();
		const auto& framebufferManager = ServiceLocator::getService<FramebufferManager>();
		const auto& glfwInitializer = ServiceLocator::getService<glfw::GlfwInitializer>();
		const auto& device = ServiceLocator::getService<DeviceManager>()->getLogicalDevice();

		glfwInitializer->handleMinimization();
		deviceManager->deviceWaitIdle();

		const auto& swapChainFramebuffers = ServiceLocator::getService<FramebufferManager>()->getSwapChainFramebuffers();
		const auto& swapChainImageViews = ServiceLocator::getService<ImageViewManager>()->getSwapChainImageViews();

		for (const auto& framebuffer : swapChainFramebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		for (const auto& imageView : swapChainImageViews)
		{
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);

		this->init();
		imageViewManager->init();
		framebufferManager->init();
	}

	void SwapChainManager::clean()
	{
		const auto& device = ServiceLocator::getService<DeviceManager>()->getLogicalDevice();

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	SwapChainSupportDetails SwapChainManager::querySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0) 
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) 
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR SwapChainManager::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		assert(!availableFormats.empty());

		for (const auto& availableFormat : availableFormats) 
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR SwapChainManager::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		// TODO: Replace with std::find.
		for (const auto& availablePresentMode : availablePresentModes) 
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) 
			{
				return availablePresentMode;
			}
		}

		// Only the VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available.
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D SwapChainManager::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const std::shared_ptr<GLFWwindow>& window)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
		{
			return capabilities.currentExtent;
		}

		int width, height;
		glfwGetFramebufferSize(window.get(), &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}
