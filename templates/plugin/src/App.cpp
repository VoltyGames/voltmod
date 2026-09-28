#include "App.hpp"

#include <VoltMod/Api.hpp>

namespace $namespace
{

void RegisterCommands(VoltMod::CommandManager& commands);

bool App::Load()
{
    Addon = Runtime.AddonManager.Add(Config.Get().addonId);
    if (Config.Get().menu.panorama)
    {
        Panorama = Runtime.UsePanorama(MenuLayout);
    }
    RegisterCommands(Runtime.Commands);
    return true;
}

}  // namespace $namespace
