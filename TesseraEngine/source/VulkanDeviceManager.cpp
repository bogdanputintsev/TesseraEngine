#include "VulkanDeviceManager.h"

#include <cassert>
#include <set>
#include <stdexcept>

#include "VulkanDebugManager.h"
#include "VulkanExtensionManager.h"
#include "VulkanQueueFamiliesManager.h"

namespace tessera::vulkan
{
	void VulkanDeviceManager::init(const std::shared_ptr<const VkInstance>& instance, const std::shared_ptr<const VkSurfaceKHR>& surface)
	{
		physicalDeviceManager.pickAnySuitableDevice(instance, surface);
		const auto physicalDevice = physicalDeviceManager.getPhysicalDevice();
		assert(physicalDevice);

		const auto [graphicsFamily, presentFamily] = VulkanQueueFamiliesManager::findQueueFamilies(*physicalDevice, surface);
		std::set uniqueQueueFamilies = { graphicsFamily.value(), presentFamily.value() };

		constexpr float queuePriority = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

		for (uint32_t queueFamily : uniqueQueueFamilies) 
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.emplace_back(queueCreateInfo);
		}

		// Specifying used device features.
		constexpr VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;

		const auto requiredExtensions = VulkanExtensionManager::getRequiredDeviceExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		// Distinction between instance and device specific validation no longer the case. This was added for back compatibility.
		const std::vector<const char*> validationLayers = VulkanDebugManager::getValidationLayers();
		if (VulkanDebugManager::validationLayersAreEnabled()) 
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else 
		{
			createInfo.enabledLayerCount = 0;
		}

		VkDevice logicalDeviceInstance = VK_NULL_HANDLE;
		if (vkCreateDevice(*physicalDevice, &createInfo, nullptr, &logicalDeviceInstance) != VK_SUCCESS)
		{
			throw std::runtime_error("VulkanLogicalDeviceManager: failed to create logical device.");
		}

		vkGetDeviceQueue(logicalDeviceInstance, graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(logicalDeviceInstance, presentFamily.value(), 0, &presentQueue);

		logicalDevice = std::make_shared<VkDevice>(logicalDeviceInstance);
	}

	void VulkanDeviceManager::clean() const
	{
		vkDestroyDevice(*logicalDevice, nullptr);
	}

}
