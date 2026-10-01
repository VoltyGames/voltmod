#pragma once

#include "Host/LoaderHandoff.hpp"
#include "Host/Plugins/CallbackList.hpp"
#include "Host/Plugins/CommandNames.hpp"
#include "Host/Plugins/LanguageTable.hpp"
#include "Host/Plugins/PublishedInterfaces.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/PerSlot.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Host/IPluginContext.hpp>
#include <VoltMod/Host/IPluginEvents.hpp>
#include <VoltMod/Host/IPluginGameData.hpp>
#include <VoltMod/Host/IPluginServices.hpp>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** A player the host has seen connect and not yet leave. */
struct ConnectedClient
{
    int64_t SteamId = 0;
    std::string Name;
    std::string Address;
    bool FullyConnected = false;
};

/** Process-wide host state behind every plugin's view. Owned by @ref PluginRegistry, which outlives every
 *  view. Game thread only; nothing locks. */
struct SharedState
{
    LoaderHandoff Start;
    IPluginGameData* GameData = nullptr;
    IPluginAddons* Addons = nullptr;  ///< null downloads nothing

    uint64_t SchemaLayoutStamp = 0;  ///< zero until the host has checked its own layout
    bool SchemaVerified = false;

    uint64_t NextToken = 1;  ///< unique across every event and the service table, never zero, never reused
    uint64_t NextOrder = 1;  ///< load positions keep rising, so a reloaded plugin dispatches last

    /** Replayed to a plugin loaded mid-map; empty until the first map starts. */
    std::string CurrentMap;
    PerSlot<std::optional<ConnectedClient>> Clients;

    CallbackList<IPluginEvents::FrameFn> Frame;
    CallbackList<IPluginEvents::ServerStartupFn> ServerStartup;
    CallbackList<IPluginEvents::ClientConnectingFn> ClientConnecting;
    CallbackList<IPluginEvents::ClientConnectedFn> ClientConnected;
    CallbackList<IPluginEvents::ClientDisconnectedFn> ClientDisconnected;
    CallbackList<IPluginEvents::ClientFullyConnectedFn> ClientFullyConnected;
    CallbackList<IPluginEvents::ClientSettingsChangedFn> ClientSettingsChanged;
    CallbackList<IPluginEvents::ConsoleCommandFn> ConsoleCommand;
    CallbackList<IPluginEvents::CheckTransmitFn> CheckTransmit;
    CallbackList<IPluginEvents::BuildGameSessionManifestFn> BuildGameSessionManifest;
    CallbackList<IPluginEvents::PlayerCommandFn> PlayerCommand;
    CallbackList<IPluginEvents::PlayerCommandFn> PlayerCommandDone;
    CallbackList<IPluginEvents::ButtonPressFn> ButtonPress;

    PublishedInterfaces Services;
    CommandNames Commands;
    LanguageTable Languages;
};

/** What a plugin had not released when it was removed. The host drops each and warns about it. */
struct LeakReport
{
    std::vector<std::string_view> Subscriptions;  ///< the event each was taken on
    std::vector<std::string> Services;
    std::vector<uint64_t> Addons;
};

/** One plugin's view of the host and the record of what it took, so every call has a known owner.
 *  Owned by @ref PluginRegistry; valid from AddPlugin until RemovePlugin. */
class PluginContext final : public IPluginContext, public IPluginEvents, public IPluginServices, public IPluginAddons
{
public:
    PluginContext(SharedState& state, std::string name, std::string logTag, std::string version, uint64_t order);

    PluginContext(const PluginContext&) = delete;
    PluginContext& operator=(const PluginContext&) = delete;

    void SetMinLogLevel(LogLevel level) { _minLevel = level; }

    /** Remove what the plugin still holds. Command names are the host's to remove, so they are not reported. */
    LeakReport RemoveAll();

    std::string_view Name() const override;
    std::string_view Version() const override;
    void* EngineInterface(const char* version) const override;
    void* ServerInterface(const char* version) const override;
    std::string_view BaseDir() const override;
    KHook::IKHook* HookDispatcher() const override;
    IPluginEvents& Events() override;
    IPluginServices& Services() override;
    IPluginLanguages& Languages() override;
    IPluginAddons& Addons() override;
    IPluginGameData* GameData() const override;
    bool RegisterCommand(std::string_view name) override;
    bool IsCommandRegistered(std::string_view name) const override;
    void WriteLog(uint8_t level, std::string_view text) override;
    uint8_t MinLogLevel() const override;
    uint64_t SchemaLayoutStamp() const override;
    bool SchemaVerified() const override;

    uint64_t OnFrame(FrameFn callback, void* context) override;
    uint64_t OnServerStartup(ServerStartupFn callback, void* context) override;
    uint64_t OnClientConnecting(ClientConnectingFn callback, void* context) override;
    uint64_t OnClientConnected(ClientConnectedFn callback, void* context) override;
    uint64_t OnClientDisconnected(ClientDisconnectedFn callback, void* context) override;
    uint64_t OnClientFullyConnected(ClientFullyConnectedFn callback, void* context) override;
    uint64_t OnClientSettingsChanged(ClientSettingsChangedFn callback, void* context) override;
    uint64_t OnConsoleCommand(ConsoleCommandFn callback, void* context) override;
    uint64_t OnCheckTransmit(CheckTransmitFn callback, void* context) override;
    uint64_t OnBuildGameSessionManifest(BuildGameSessionManifestFn callback, void* context) override;
    uint64_t OnPlayerCommand(PlayerCommandFn callback, void* context) override;
    uint64_t OnPlayerCommandDone(PlayerCommandFn callback, void* context) override;
    uint64_t OnButtonPress(ButtonPressFn callback, void* context) override;

    void Publish(std::string_view name, void* implementation) override;
    void Unpublish(std::string_view name) override;
    void* Find(std::string_view name) override;

    bool Add(uint64_t addonId) override;
    void Remove(uint64_t addonId) override;
    bool IsReady(int slot) override;

    /** Raise on this plugin alone what it missed by loading mid-map: OnServerStartup for the running
     *  map, then OnClientConnected and OnClientFullyConnected for each player already in. */
    void ReplayMissedEvents();
    /** Raise OnClientDisconnected on this plugin alone for each player still in, before a mid-map unload. */
    void DisconnectClients();

    /** A token this plugin never took is ignored. */
    void Unsubscribe(uint64_t token) override;

private:
    struct Subscribed
    {
        uint64_t Token = 0;
        std::string_view Event;        ///< names it in a leak line
        std::function<void()> Remove;  ///< drops the token from the list it went into
    };

    template <class Fn>
    uint64_t Subscribe(std::string_view event, CallbackList<Fn>& list, Fn callback, void* context);

    SharedState& _state;
    std::string _name;
    std::string _logTag;
    std::string _version;
    LogLevel _minLevel = LogLevel::Info;
    uint64_t _order = 0;
    std::vector<Subscribed> _subscriptions;  ///< in the order the plugin took them
    std::vector<uint64_t> _addons;           ///< one entry per Add
};

}  // namespace VoltMod
