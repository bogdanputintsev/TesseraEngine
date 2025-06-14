#pragma once

#include "Math.h"

namespace parus::math
{

    struct alignas(16) GlobalUbo
    {
        alignas(16) TrivialMatrix4x4 view; 
        alignas(16) TrivialMatrix4x4 projection;
        TrivialVector3 cameraPosition;
        int debug = 0;
    };

    struct alignas(16) InstanceUbo
    {
        alignas(16) TrivialMatrix4x4 model;
        alignas(16) TrivialMatrix4x4 normal;
    };

    struct alignas(16) DirectionalLightUbo
    {
        TrivialVector3 color;
        TrivialVector3 direction;
    };
    
    // struct LightUbo
    // {
    //     int lightCount;
    //     TrivialVector3 color[vulkan::MAX_NUMBER_OF_LIGHTS];
    //     float intensity[vulkan::MAX_NUMBER_OF_LIGHTS];
    //     vulkan::LightType type[vulkan::MAX_NUMBER_OF_LIGHTS];
    //
    //     // Directional light only
    //     TrivialVector3 direction[vulkan::MAX_NUMBER_OF_LIGHTS];
    //
    //     // Point light only
    //     TrivialVector3 position[vulkan::MAX_NUMBER_OF_LIGHTS];
    //     float radius[vulkan::MAX_NUMBER_OF_LIGHTS];
    // };
    
}
