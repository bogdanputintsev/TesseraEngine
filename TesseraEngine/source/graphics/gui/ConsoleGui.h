﻿#pragma once
#include <string>

struct ImGuiInputTextCallbackData;

namespace tessera::imgui
{
    
    class ConsoleGui final
    {
    public:
        void registerEvents();
        void draw();

    private:
        void onNewCommandSent();
        static std::string processCommand(const std::string& inputCommand);
        
        std::string commandLineText;
        std::string consoleHistory = "Tessera Engine v0.3.0\n";
        
        bool isVisible{false};
    };
}
