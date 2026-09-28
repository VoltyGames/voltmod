#pragma once

#include "Host/Plugins/PluginContext.hpp"

#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Host/IPluginGameData.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/**
 * @brief The loaded plugins and the engine events raised on them, one per process.
 *
 * Owns the @ref SharedState. Game thread only; nothing locks. SDK-free so it is unit-tested without the engine.
 */
class PluginRegistry
{
public:
    explicit PluginRegistry(LoaderHandoff start = {}, IPluginGameData* gameData = nullptr,
                            IPluginAddons* addons = nullptr);
    ~PluginRegistry();

    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    /** Give @p name its own view, at the end of the dispatch order. Nullptr when it already has one.
     *  @p logTag prefixes every line the plugin writes; empty means its name. */
    PluginContext* AddPlugin(std::string_view name, std::string_view logTag = {}, std::string_view version = {});

    /** Drop every subscription, publication and command name @p name still holds, and report them. */
    LeakReport RemovePlugin(std::string_view name);

    PluginContext* FindPlugin(std::string_view name);

    /** What the loader handed over: the factories, the hook dispatcher and the game directory. */
    const LoaderHandoff& Start() const { return _state.Start; }
    /** Between ClientConnected and ClientDisconnected on @p slot. */
    bool IsConnected(int slot) const { return IsValidSlot(slot) && _state.Clients[slot].has_value(); }

    /** The one gamedata resolution every plugin binds from, null when the host has none. */
    IPluginGameData* GameData() const { return _state.GameData; }

    /** Record what the host's own schema check found. @p stamp identifies the layout it checked,
     *  so a plugin can tell whether the answer covers the offsets it was built with. */
    void SetSchemaLayout(uint64_t stamp, bool verified);

    void RaiseFrame();
    void RaiseServerStartup(std::string_view mapName);
    /** Ask each plugin whether @p slot may join. The first refusal's reason, or empty to admit. */
    std::string RaiseClientConnecting(int slot, int64_t steamId, std::string_view name);
    void RaiseClientConnected(int slot, int64_t steamId, std::string_view name, std::string_view address);
    void RaiseClientDisconnected(int slot);
    void RaiseClientFullyConnected(int slot);
    void RaiseClientSettingsChanged(int slot);
    /** True when a plugin answered it: later plugins never see it and the engine call is blocked. */
    bool RaiseConsoleCommand(std::string_view name, std::string_view arguments, int slot);
    void RaiseCheckTransmit(CCheckTransmitInfo** infoList, int infoCount);
    void RaiseBuildGameSessionManifest(IEntityResourceManifest* manifest);

    /** Keep @p name for the host's own console commands, so no plugin can take it. */
    void RegisterHostCommand(std::string_view name);

private:
    SharedState _state;
    std::vector<std::unique_ptr<PluginContext>> _plugins;  ///< in load order
};

}  // namespace VoltMod
