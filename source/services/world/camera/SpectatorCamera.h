#pragma once

#include "engine/utils/math/Math.h"

namespace parus
{
    
    class SpectatorCamera
    {
    public:
        void updateTransform(float deltaTime);

        inline math::Vector3 getPosition() const { return position; }
        inline math::Vector3 getForwardVector() const { return forward; }
        inline math::Vector3 getUpVector() const { return up; }
        inline math::Vector3 getRightVector() const { return right; }
        
    private:
        math::Vector3 position = math::Vector3(65.8f, 55.4f, -45.32f);
        math::Vector3 forward = math::Vector3(0.0f, 0.0f, -1.0f);
        math::Vector3 up = math::Vector3(0.0f, 1.0f, 0.0f);
        math::Vector3 right = math::Vector3(1.0f, 0.0f, 0.0f);

        float yaw = -90.0f;
        float pitch = 0.0f;
        float speed = 20.5f;
        float sensitivity = 0.25f;

        float speedAccelerationMultiplier = 3.5f;
    };
    
}
