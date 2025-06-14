#include "SpectatorCamera.h"

#include <algorithm>

#include "engine/input/Input.h"
#include "services/Services.h"

namespace parus
{
    void SpectatorCamera::updateTransform(const float deltaTime)
    {
        const auto input = Services::get<Input>();
        
        // Process keyboard
        float velocity = speed * deltaTime;
        if (input->isKeyPressed(KeyButton::KEY_SHIFT))
        {
            velocity *= speedAccelerationMultiplier;
        }
        if (input->isKeyPressed(KeyButton::KEY_W))
        {
            position += forward * velocity;
        }
        if (input->isKeyPressed(KeyButton::KEY_S))
        {
            position -= forward * velocity;
        }
        if (input->isKeyPressed(KeyButton::KEY_A))
        {
            position -= right * velocity;
        }
        if (input->isKeyPressed(KeyButton::KEY_D))
        {
            position += right * velocity;
        }
        if (input->isKeyPressed(KeyButton::KEY_E))
        {
            position += up * velocity;
        }
        if (input->isKeyPressed(KeyButton::KEY_Q))
        {
            position -= up * velocity;
        }
        
        // Process mouse.
        if (input->isMouseDown(MouseButton::BUTTON_RIGHT))
        {
            auto [mouseOffsetX, mouseOffsetY] = input->getMouseOffset();
            const float offsetX = static_cast<float>(mouseOffsetX) * sensitivity;
            const float offsetY = static_cast<float>(mouseOffsetY) * sensitivity;

            yaw += offsetX;
            pitch = std::clamp(pitch + offsetY, -90.0f, 90.0f);
        }
        
        // Calculate new direction vector.
        const math::Vector3 direction {
            sin(math::radians(yaw) * cos(math::radians(pitch))),
            sin(math::radians(pitch)),
            -cos(math::radians(yaw)) * cos(math::radians(pitch))
        };

        forward = direction.normalize();
        
        // Recalculate right and up vectors.
        right = forward.cross(math::Vector3::up()).normalize();
        up = right.cross(forward).normalize();
    }
    
}
