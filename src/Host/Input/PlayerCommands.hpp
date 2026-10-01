#pragma once

#include "Host/Plugins/PluginRegistry.hpp"

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/PlayerInput.hpp>

namespace VoltMod
{

/**
 * @brief The RunCommand hook for every plugin: each command is decoded once, and only while a
 * plugin listens. Game thread only.
 */
class PlayerCommands
{
public:
    /** @p bindings and @p registry must outlive this. */
    PlayerCommands(const Bindings& bindings, PluginRegistry& registry);

    PlayerCommands(const PlayerCommands&) = delete;
    PlayerCommands& operator=(const PlayerCommands&) = delete;

private:
    void Before(void* movementServices, const void* userCmd);
    void After();
    void Decode(const void* userCmd);

    PluginRegistry& _registry;
    const Bindings& _bindings;
    PlayerInput _input;  ///< decoded before RunCommand, raised again after it
    int _slot = -1;      ///< -1 when nothing was decoded; RunCommand does not nest
    Subscription _hook;
};

}  // namespace VoltMod
