#pragma once
#include <vulkan/vulkan_core.h>

namespace parus::vulkan
{
    
    struct VulkanStorage final
    {
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        // Debug Manager
        // ReSharper disable once CppInconsistentNaming
        PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

    };

}
