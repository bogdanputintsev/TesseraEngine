#pragma once

#include <string>
#include <vulkan/vulkan_core.h>

#include "services/renderer/vulkan/storage/VulkanStorage.h"

namespace parus::vulkan::utils
{

    uint32_t findMemoryType(const VulkanStorage& storage, const uint32_t typeFilter, const VkMemoryPropertyFlags properties);

    void setDebugObjectName(const VulkanStorage& storage, void* objectHandle, VkObjectType objectType, const std::string& name);

}
