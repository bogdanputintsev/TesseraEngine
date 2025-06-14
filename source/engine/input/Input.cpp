#include "Input.h"

#include "engine/Event.h"

namespace parus
{

    void Input::processKey(const KeyButton key, const bool isPressed)
    {
        const int keyCode = static_cast<int>(key);
        
        if (keyboardState.keyPressed[keyCode] != isPressed)
        {
            keyboardState.keyPressed[keyCode] = isPressed;
            
            FIRE_EVENT(isPressed
                ? EventType::EVENT_KEY_PRESSED
                : EventType::EVENT_KEY_RELEASED,
                key);
            
            LOG_DEBUG("Key " + std::string(toString(key)) + " is " + (isPressed ? "pressed" : "released"));
        }
    }

    void Input::processChar(const char inputChar)
    {
        FIRE_EVENT(parus::EventType::EVENT_CHAR_INPUT, inputChar);
        LOG_DEBUG("Char entered: " + std::string(1, inputChar));
    }

    void Input::processButton(const MouseButton button, const bool isPressed)
    {
        const int buttonCode = static_cast<int>(button);

        if (mouseState.mousePressed[buttonCode] != isPressed)
        {
            mouseState.mousePressed[buttonCode] = isPressed;

            FIRE_EVENT(isPressed
                ? EventType::EVENT_MOUSE_BUTTON_PRESSED
                : EventType::EVENT_MOUSE_BUTTON_RELEASED,
                button);
        
            LOG_DEBUG("Mouse " + std::string(toString(button)) + " is " + (isPressed ? "pressed" : "released"));
        }
    }

    void Input::processMouseMove(const int newMouseX, const int newMouseY)
    {
        if (mouseState.mouseX != newMouseX || mouseState.mouseY != newMouseY)
        {
            mouseState.offsetX = newMouseX - mouseState.mouseX;
            mouseState.offsetY = mouseState.mouseY - newMouseY;
            mouseState.mouseX = newMouseX;
            mouseState.mouseY = newMouseY;

            FIRE_EVENT(EventType::EVENT_MOUSE_MOVED, newMouseX, newMouseY);
        }
        
        // LOG_DEBUG("Mouse position: " + std::to_string(mouseState.mouseX) + ", " + std::to_string(mouseState.mouseY));
        // LOG_DEBUG("Mouse delta: " + std::to_string(mouseState.offsetX) + ", " + std::to_string(mouseState.offsetY));
    }

    void Input::processMouseWheel(const int wheelDelta)
    {
        FIRE_EVENT(EventType::EVENT_MOUSE_WHEEL, wheelDelta);
    }

    bool Input::isKeyPressed(const KeyButton key) const
    {
        const int keyCode = static_cast<int>(key);
        return keyboardState.keyPressed[keyCode];
    }

    bool Input::isMouseDown(const MouseButton button) const
    {
        const int buttonCode = static_cast<int>(button);
        return mouseState.mousePressed[buttonCode];
    }

    std::pair<int, int> Input::getMouseOffset()
    {
        // Check if the mouse offset hasn't changed since the last call.
        if (mouseState.lastOffsetX == mouseState.offsetX && mouseState.lastOffsetY == mouseState.offsetY)
        {
            // If there's no change, reset the offset values to zero.
            // This ensures that the mouse offset is only reported when there's actual movement.
            mouseState.offsetX = 0;
            mouseState.offsetY = 0;
        }

        // Update the last recorded mouse offset with the current offset values.
        // This is necessary to track changes in the next call.
        mouseState.lastOffsetX = mouseState.offsetX;
        mouseState.lastOffsetY = mouseState.offsetY;

        // Return the current mouse offset as a pair of integers (X, Y)
        // This represents the movement of the mouse since the last call
        return { mouseState.offsetX, mouseState.offsetY };
    }

}
