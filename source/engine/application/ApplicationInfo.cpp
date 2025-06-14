#include "ApplicationInfo.h"

#include <filesystem>


namespace parus
{
    void ApplicationInfo::readAll()
    {
        if (!std::filesystem::exists(APPLICATION_INFO_FILE_PATH))
        {
            saveAll();
            DEBUG_ASSERT(std::filesystem::exists(APPLICATION_INFO_FILE_PATH), "Save application info method didn't work as expected.");
            return;
        }

        utils::readFile(APPLICATION_INFO_FILE_PATH, [&](std::ifstream& in)
        {
            uint32_t nameLength = 0;
            in.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));

            generalInfo.applicationName.resize(nameLength);
            in.read(generalInfo.applicationName.data(), nameLength);

            in.read(reinterpret_cast<char*>(&generalInfo.versionMajor), sizeof(generalInfo.versionMajor));
            in.read(reinterpret_cast<char*>(&generalInfo.versionMinor), sizeof(generalInfo.versionMinor));
            in.read(reinterpret_cast<char*>(&generalInfo.versionPatch), sizeof(generalInfo.versionPatch));
        });
    }

    [[maybe_unused]] void ApplicationInfo::saveAll() const
    {
        utils::writeFile(APPLICATION_INFO_FILE_PATH, [&](std::ofstream& out)
        {
            const uint32_t nameLength = static_cast<uint32_t>(generalInfo.applicationName.size());
            out.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
            out.write(generalInfo.applicationName.c_str(), nameLength);
            out.write(reinterpret_cast<const char*>(&generalInfo.versionMajor), sizeof(generalInfo.versionMajor));
            out.write(reinterpret_cast<const char*>(&generalInfo.versionMinor), sizeof(generalInfo.versionMinor));
            out.write(reinterpret_cast<const char*>(&generalInfo.versionPatch), sizeof(generalInfo.versionPatch));
        });
    }
}
