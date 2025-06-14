#include "Material.h"

#include "engine/EngineCore.h"
#include "services/Services.h"
#include "services/world/World.h"

namespace parus::vulkan
{
    Material::Material()
    {
        iterateAllTextureTypes([&](const TextureType textureType)
        {
            textures[textureType] = Services::get<World>()->getStorage()->getDefaultTextureOfType(textureType);
        });
    }

    void Material::addOrUpdateTexture(const TextureType textureType, const std::shared_ptr<VulkanTexture>& newTexture)
    {
        if (!newTexture)
        {
            const auto defaultTexture = Services::get<World>()->getStorage()->getDefaultTextureOfType(textureType);
            textures.insert_or_assign(textureType, defaultTexture);
            return;
        }
        
        textures.insert_or_assign(textureType, newTexture);
    }

    std::shared_ptr<VulkanTexture> Material::getTexture(const TextureType textureType)
    {
        DEBUG_ASSERT(textures.contains(textureType), "Texture of each type must always exist.");
        return textures.at(textureType);
    }

    std::vector<std::shared_ptr<const VulkanTexture>> Material::getAllTextures() const
    {
        std::vector<std::shared_ptr<const VulkanTexture>> allTextures;
        allTextures.reserve(NUMBER_OF_TEXTURE_TYPES);

        for (TextureType textureType : ALL_TEXTURE_TYPES)
        {
            DEBUG_ASSERT(textures.contains(textureType), "Material must always contain all types of textures.");
            allTextures.push_back(textures.at(textureType));
        }

        return allTextures;
    }

    void Material::iterateAllTextures(const std::function<void(const TextureType, const std::shared_ptr<const VulkanTexture>&)>& callback) const
    {
        const auto allTextures = getAllTextures();

        ASSERT(allTextures.size() == NUMBER_OF_TEXTURE_TYPES, "Some texture types are missing in material.");
        
        for (size_t i = 0; i < NUMBER_OF_TEXTURE_TYPES; i++)
        {
            callback(ALL_TEXTURE_TYPES[i], allTextures[i]);
        }
    }

    void Material::iterateAllTextureTypes(const std::function<void(TextureType)>& callback)
    {
        for (const TextureType type : ALL_TEXTURE_TYPES)
        {
            callback(type);
        }
    }

    void Material::iterateAllTextureTypes(const std::function<void(int, TextureType)>& callback)
    {
        for (int i = 0; i < static_cast<int>(NUMBER_OF_TEXTURE_TYPES); i++)
        {
            callback(i, ALL_TEXTURE_TYPES[i]);
        }
    }
}
