#pragma once
#include <mutex>
#include <string>
#include <unordered_map>

#include "services/renderer/vulkan/material/Material.h"
#include "services/renderer/vulkan/mesh/Mesh.h"

namespace parus
{
    
    class Storage final
    {
    public:
        // --- Materials ---
        void addMaterial(const std::string& materialName, const std::shared_ptr<vulkan::Material>& newMaterial);
        std::shared_ptr<vulkan::Material> getOrLoadMaterial(
            const std::string& materialName,
            const std::string& diffuseTexturePath,
            const std::string& normalTexturePath = "",
            const std::string& metallicTexturePath = "",
            const std::string& roughnessTexturePath = "",
            const std::string& ambientOcclusionTexturePath = "");
        bool hasMaterial(const std::string& materialName) const;
        std::shared_ptr<vulkan::Material> getMaterial(const std::string& materialName);
        std::shared_ptr<vulkan::Material> getDefaultMaterial();
        std::vector<std::shared_ptr<vulkan::Material>> getAllMaterials() const;
        
        // --- Textures ---
        void addNewTexture(const std::string& path, const std::shared_ptr<vulkan::VulkanTexture>& newTexture);
        std::shared_ptr<vulkan::VulkanTexture> getTexture(const std::string& path);
        std::shared_ptr<vulkan::VulkanTexture> getDefaultTextureOfType(const vulkan::TextureType textureType);
        std::shared_ptr<vulkan::VulkanTexture> getOrLoadTexture(const std::string& texturePath);
        bool hasTexture(const std::string& path) const;
        std::vector<std::shared_ptr<vulkan::VulkanTexture>> getAllTextures() const;

        // --- Meshes ---
        void addNewMesh(const std::string& path, const std::shared_ptr<Mesh>& newMesh);
        std::shared_ptr<Mesh> getMeshByPath(const std::string& path);
        std::vector<std::shared_ptr<Mesh>> getAllMeshes() const;
        std::vector<std::shared_ptr<Mesh>> getAllMeshesByType(const MeshType meshType) const;

    private:
        std::unordered_map<std::string, std::shared_ptr<vulkan::Material>> materials;
        std::shared_ptr<vulkan::Material> defaultMaterial;

        mutable std::mutex materialMutex;

        
        std::unordered_map<std::string, std::shared_ptr<vulkan::VulkanTexture>> textures;
        std::unordered_map<vulkan::TextureType, std::shared_ptr<vulkan::VulkanTexture>> defaultTextures;
        void fillDefaultTextures();
        mutable std::mutex texturesMutex;
        
        
        std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;
        mutable std::mutex meshesMutex;
    };

    
}
