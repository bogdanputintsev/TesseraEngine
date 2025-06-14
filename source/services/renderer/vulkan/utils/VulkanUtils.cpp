#include "VulkanUtils.h"
#include "engine/EngineCore.h"

#include <optional>


namespace parus::vulkan::utils
{

    uint32_t findMemoryType(const VulkanStorage& storage, const uint32_t typeFilter, const VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(storage.physicalDevice, &memoryProperties);

        std::optional<uint32_t> memoryIndex;

        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        {
            if (typeFilter & 1 << i && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                memoryIndex = i;
                break;
            }
        }

        ASSERT(memoryIndex.has_value(), "Failed to find suitable memory type.");
        return memoryIndex.value();
    }

    void setDebugObjectName(const VulkanStorage& storage, void* objectHandle, const VkObjectType objectType, const std::string& name)
    {
        if (storage.vkSetDebugUtilsObjectNameEXT)
        {
            VkDebugUtilsObjectNameInfoEXT nameInfo{};
            nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            nameInfo.objectType = objectType;
            nameInfo.objectHandle = reinterpret_cast<uint64_t>(objectHandle);
            nameInfo.pObjectName = name.c_str();

            // Call our callback function.
            storage.vkSetDebugUtilsObjectNameEXT(storage.device, &nameInfo);
        }
    }
    
}
