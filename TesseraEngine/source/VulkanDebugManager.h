#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "TesseraLog.h"

namespace tessera::vulkan
{

	class VulkanDebugManager
	{
	public:
		void init(const std::shared_ptr<const VkInstance>& instance);

		static void populate(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

		void clean(const std::shared_ptr<const VkInstance>& instance) const;

		static void checkValidationLayerSupport();

		static bool validationLayersAreEnabled();

		static std::vector<const char*> getValidationLayers();

	private:
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData);

		static LogType getLogType(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity);

		static VkResult createDebugUtilsMessengerExt(const std::shared_ptr<const VkInstance>& instance,
		                                             const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		                                             const VkAllocationCallbacks* pAllocator,
		                                             VkDebugUtilsMessengerEXT* pDebugMessenger);

		static void destroyDebugUtilsMessengerExt(const std::shared_ptr<const VkInstance>& instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks
		                                          * pAllocator);

		VkDebugUtilsMessengerEXT debugMessenger = nullptr;

	};

}


