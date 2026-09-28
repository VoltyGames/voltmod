#include "Host/Files/ServerAssets.hpp"

#include <VoltMod/Core/Files/Paths.hpp>
#include <filesystem.h>
#include <filesystem>
#include <system_error>

namespace VoltMod
{

void ServerAssets::Mount(std::string_view plugin)
{
    const std::filesystem::path folder = ResolvePath(PluginFile(plugin, "server-assets"));
    std::error_code ignored;
    if (!_files || !std::filesystem::is_directory(folder, ignored))
    {
        return;
    }
    const std::string path = folder.generic_string();
    _files->AddSearchPath(path.c_str(), "GAME", PATH_ADD_TO_HEAD, SEARCH_PATH_PRIORITY_VPK);
    _mounted.insert_or_assign(std::string(plugin), path);
}

void ServerAssets::Unmount(std::string_view plugin)
{
    const auto found = _mounted.find(plugin);
    if (found == _mounted.end())
    {
        return;
    }
    _files->RemoveSearchPath(found->second.c_str(), "GAME");
    _mounted.erase(found);
}

}  // namespace VoltMod
