#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <GLFW/glfw3.h>

#include "VulkanPhysicalDeviceManager.h"

namespace tessera::vulkan
{

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		[[nodiscard]] inline bool isComplete() const;
	};

	class VulkanQueueFamiliesManager final
	{
	public:
		static QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice& physicalDevice, const std::shared_ptr<const VkSurfaceKHR>& surface);
	};

}

