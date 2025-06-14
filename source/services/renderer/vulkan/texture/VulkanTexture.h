#pragma once

#include <filesystem>
#include <vulkan/vulkan_core.h>

#include "engine/utils/math/Math.h"


namespace parus::vulkan
{
    struct VulkanTexture
    {
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
        VkSampler sampler;
        uint32_t maxMipLevels;
    };

    VulkanTexture importTextureFromFile(const std::string& filePath);
    VulkanTexture createSolidColorTexture(const math::Vector3& color);
}
