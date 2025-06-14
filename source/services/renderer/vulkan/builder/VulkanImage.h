#pragma once
#include <string>
#include <vulkan/vulkan_core.h>

#include "services/renderer/vulkan/storage/VulkanStorage.h"

namespace parus::vulkan
{

    class VulkanImage final
    {
    public:
        VkImage image;
        VkDeviceMemory deviceMemory;
        VkImageView imageView;
        VkSampler sampler;

        class Builder;
    };

    class VulkanImage::Builder final
    {
    public:
        explicit Builder(std::string debugName);
        [[nodiscard]] VulkanImage build(const VulkanStorage& storage) const;

        Builder& setExtent(const uint32_t newWidth, const uint32_t newHeight, const uint32_t newDepth = 1);
        Builder& setMipLevels(const uint32_t newMipLevels);
        Builder& setArrayLayers(const uint32_t newArrayLayers);
        Builder& setFormat(const VkFormat newFormat);
        Builder& setFlags(const VkImageCreateFlags newFlags);

    private:
        std::string debugName;

        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkImageCreateFlags flags = 0;
        
    };
    
}
