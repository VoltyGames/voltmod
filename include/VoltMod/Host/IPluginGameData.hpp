#pragma once

#include <VoltMod/Engine/GameData/GameDataLocation.hpp>
#include <string_view>

namespace VoltMod
{

/** Gamedata the host resolved once before any plugin loaded; fixed after startup. Game thread only. */
struct IPluginGameData
{
    /** What the host resolved for @p name, when @p sections admits the section holding it. */
    virtual GameDataLocation Lookup(GameDataSection sections, std::string_view name) = 0;

protected:
    ~IPluginGameData() = default;
};

}  // namespace VoltMod
