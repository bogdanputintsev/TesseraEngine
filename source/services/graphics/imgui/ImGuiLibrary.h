#pragma once

#include <vulkan/vulkan_core.h>

#pragma warning(push, 0)
#include "third-party/imgui/imgui.h"
#pragma warning(pop)

#include "services/graphics/GraphicsLibrary.h"
#include "services/graphics/gui/ConsoleGui.h"


namespace parus
{
    enum class KeyButton : uint8_t;
}


namespace parus::imgui
{
    class ImGuiLibrary final : public GraphicsLibrary
    {
    public:
        void init() override;
        void drawFrame() override;
        void draw();
        static void renderDrawData(VkCommandBuffer cmd);
        void handleMinimization() override;
        [[nodiscard]] std::vector<const char*> getRequiredExtensions() const override;
        void clean() override;
    private:
        static ImGuiKey getImGuiKeyCode(const ::parus::KeyButton keyTest);

        ConsoleGui consoleGui{};
    };


}
