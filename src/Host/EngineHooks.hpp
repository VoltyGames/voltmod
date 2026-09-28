#pragma once

#include "Host/EngineInterfaces.hpp"
#include "Host/Plugins/PluginRegistry.hpp"

#include <VoltMod/Core/Signals/Subscriptions.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <cstdint>
#include <functional>
#include <string_view>

namespace VoltMod
{

/**
 * @brief The engine hooks, installed once for the whole process.
 *
 * Each one raises the matching event on @ref PluginRegistry, which calls every loaded plugin in load
 * order. Game thread only, like everything downstream of it.
 */
class EngineHooks
{
public:
    /**
     * @param beforeFrame Runs at the start of every frame, before the frame reaches the plugins.
     * @param beforeServerStartup Runs on every map change, before the plugins hear about it.
     * @param beforeClientConnected Runs with each connecting client's SteamID, before the plugins hear about it.
     */
    EngineHooks(PluginRegistry& host, const EngineInterfaces& engine, std::function<void()> beforeFrame,
                std::function<void()> beforeServerStartup, std::function<void(int64_t)> beforeClientConnected);

    EngineHooks(const EngineHooks&) = delete;
    EngineHooks& operator=(const EngineHooks&) = delete;

private:
    void ConnectClient(int slot, uint64_t xuid, std::string_view name, std::string_view address);

    /** A map change moves clients to new slots; ClientPutInServer reconnects them there. */
    void DisconnectEveryone();

    PluginRegistry& _host;
    std::function<void()> _beforeFrame;
    std::function<void()> _beforeServerStartup;
    std::function<void(int64_t)> _beforeClientConnected;
    Subscriptions _hooks;
};

}  // namespace VoltMod
