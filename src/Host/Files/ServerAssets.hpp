#pragma once

#include <VoltMod/Engine/EngineTypes.hpp>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace VoltMod
{

/** Each plugin's `server-assets` folder on the game's search path while the plugin is loaded, ahead of
 *  the game's VPKs, which beat loose files under `game/csgo`. Weapon subclasses apply from the next map. */
class ServerAssets
{
public:
    /** Null @p files mounts nothing. */
    explicit ServerAssets(IFileSystem* files) : _files(files) {}

    void Mount(std::string_view plugin);
    void Unmount(std::string_view plugin);

private:
    IFileSystem* _files;
    std::map<std::string, std::string, std::less<>> _mounted;  ///< plugin -> folder
};

}  // namespace VoltMod
