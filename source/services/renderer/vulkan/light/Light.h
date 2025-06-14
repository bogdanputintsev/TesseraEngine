#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

#include "engine/utils/math/Math.h"
#include "engine/utils/math/UniformBufferObjects.h"

namespace parus::vulkan
{
    // static constexpr size_t MAX_NUMBER_OF_LIGHTS = 16;
    //
    // enum class LightType :  uint32_t
    // {
    //     DIRECTIONAL = 0,
    //     POINT = 1,
    // };
    
    // struct Light
    // {
    //     // Common parameters
    //     math::TrivialVector3 color;
    //     float intensity;
    //     LightType type;
    //
    //     // Directional light only
    //     math::TrivialVector3 direction;
    //
    //     // Point light only
    //     math::TrivialVector3 position;
    //     float radius;
    // };

    struct VulkanDirectionalLight
    {
        math::DirectionalLightUbo light;
        std::vector<VkDescriptorSet> descriptorSets;
    };
}
