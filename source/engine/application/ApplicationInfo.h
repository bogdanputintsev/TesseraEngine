#pragma once
#include <string>

#include "engine/utils/Utils.h"

namespace parus
{
    
    struct ApplicationInfo
    {
    public:
        struct GeneralInfo
        {
            std::string applicationName = "Parus Engine";
            uint8_t versionMajor = 0;
            uint8_t versionMinor = 3;
            uint8_t versionPatch = 0;
        } generalInfo;
        
        struct WindowInfo
        {
            std::string windowTitle = "Parus";
            uint32_t positionX = 250;
            uint32_t positionY = 250;
            uint32_t width = 1200;
            uint32_t height = 900;
            bool isMinimized = false;
        } windowInfo;

        void readAll();
        void saveAll() const;

    private:
        static constexpr auto APPLICATION_INFO_FILE_PATH = "bin/config/parus.about";
        
    };
    
}
