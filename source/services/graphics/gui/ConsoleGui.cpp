#include "ConsoleGui.h"

#pragma warning(push, 0)
#include "third-party/imgui/imgui.h"
#include "third-party/imgui/imgui_internal.h"
#pragma warning(pop)

#include "engine/Event.h"
#include "engine/input/Input.h"
#include "services/Services.h"
#include "services/world/World.h"

namespace parus::imgui
{
    void ConsoleGui::registerEvents()
    {
        REGISTER_EVENT(EventType::EVENT_KEY_PRESSED, [&](const KeyButton key)
        {
           if (key == KeyButton::KEY_GRAVE)
           {
               isVisible = !isVisible;
           }
        });
    }

    void ConsoleGui::draw()
    {
        if (isVisible)
        {
            // ImGui::SetNextWindowPos(ImVec2(0, 0));
            // ImGui::SetNextWindowSize(ImVec2(250, ImGui::GetTextLineHeight() * 19));
            ImGui::SetNextWindowBgAlpha(0.1f);
            ImGui::Begin("Console", nullptr);
            {
                commandLineText.reserve(50);
                consoleHistory.reserve(1024);
                
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SetKeyboardFocusHere();
                if (ImGui::InputText("##command", commandLineText.data(), commandLineText.capacity() + 1))
                {
                    commandLineText.resize(strlen(commandLineText.data()));
                }
                
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    onNewCommandSent();
                }
                
                ImGui::InputTextMultiline("##console", consoleHistory.data(), consoleHistory.capacity() + 1,
                    ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16),
                    ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::End();
        }
    }

    void ConsoleGui::onNewCommandSent()
    {
        if (!commandLineText.empty())
        {
            consoleHistory += "> " + commandLineText + "\n";
            consoleHistory += processCommand(commandLineText);
            consoleHistory.resize(strlen(consoleHistory.data()));
            commandLineText = "";
        }
    }

    std::string ConsoleGui::processCommand(const std::string& inputCommand)
    {
        std::string consoleOutput;
        
        if (strcmp(inputCommand.c_str(), "get camera position") == 0)
        {
            const auto cameraPosition { Services::get<World>()->getMainCamera().getPosition() };
            consoleOutput += "Camera position:"
                "\n\tX: " + std::to_string(cameraPosition.x) +
                "\n\tY: " + std::to_string(cameraPosition.y) +
                "\n\tZ: " + std::to_string(cameraPosition.z);
        }
        else if (strcmp(inputCommand.c_str(), "import mesh") == 0)
        {
            
        }
        else
        {
            consoleOutput += "Unknown command: '" + inputCommand + "'.";
        }

        return consoleOutput + "\n";
    }
}
