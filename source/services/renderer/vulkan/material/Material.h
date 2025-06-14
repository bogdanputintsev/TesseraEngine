#pragma once
#include <array>
#include <functional>
#include <unordered_map>

#include "services/renderer/vulkan/texture/VulkanTexture.h"

namespace parus::vulkan
{
    
    enum class TextureType
    {
        ALBEDO,
        NORMAL,
        METALLIC,
        ROUGHNESS,
        AMBIENT_OCCLUSION,
    };

    static constexpr std::array ALL_TEXTURE_TYPES = {
        TextureType::ALBEDO,
        TextureType::NORMAL,
        TextureType::METALLIC,
        TextureType::ROUGHNESS,
        TextureType::AMBIENT_OCCLUSION
    };

    static constexpr size_t NUMBER_OF_TEXTURE_TYPES = ALL_TEXTURE_TYPES.size();
    
    class Material final
    {
    public:
        Material();
        
        void addOrUpdateTexture(TextureType textureType, const std::shared_ptr<VulkanTexture>& newTexture);
        std::shared_ptr<VulkanTexture> getTexture(const TextureType textureType);

        [[nodiscard]] std::vector<std::shared_ptr<const VulkanTexture>> getAllTextures() const;
        void iterateAllTextures(const std::function<void(const TextureType, const std::shared_ptr<const VulkanTexture>&)>& callback) const;
        static void iterateAllTextureTypes(const std::function<void(TextureType)>& callback);
        static void iterateAllTextureTypes(const std::function<void(int, TextureType)>& callback);

        VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE;
    private:
        std::unordered_map<TextureType, std::shared_ptr<VulkanTexture>> textures;
    };
    
}

