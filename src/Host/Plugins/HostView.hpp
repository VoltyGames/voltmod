#pragma once

#include "Host/HostStart.hpp"
#include "Host/Plugins/CallbackList.hpp"
#include "Host/Plugins/CommandNames.hpp"
#include "Host/Plugins/LanguageTable.hpp"
#include "Host/Plugins/ServiceTable.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/PerSlot.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Host/IHost.hpp>
#include <VoltMod/Host/IHostEvents.hpp>
#include <VoltMod/Host/IHostGameData.hpp>
#include <VoltMod/Host/IHostServices.hpp>
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

/** Process-wide host state behind every plugin's view. Owned by @ref PluginHost, which outlives every
 *  view. Game thread only; nothing locks. */
struct HostState
{
    HostStart Start;
    IHostGameData* GameData = nullptr;

    uint64_t SchemaLayoutStamp = 0;  ///< zero until the host has checked its own layout
    bool SchemaVerified = false;

    uint64_t NextToken = 1;  ///< unique across every event and the service table, never zero, never reused
    uint64_t NextOrder = 1;  ///< load positions keep rising, so a reloaded plugin dispatches last

    /** Replayed to a plugin loaded mid-map; empty until the first map starts. */
    std::string CurrentMap;
    PerSlot<std::optional<ConnectedClient>> Clients;

    CallbackList<IHostEvents::FrameFn> Frame;
    CallbackList<IHostEvents::ServerStartupFn> ServerStartup;
    CallbackList<IHostEvents::ClientConnectingFn> ClientConnecting;
    CallbackList<IHostEvents::ClientConnectedFn> ClientConnected;
    CallbackList<IHostEvents::ClientDisconnectedFn> ClientDisconnected;
    CallbackList<IHostEvents::ClientFullyConnectedFn> ClientFullyConnected;
    CallbackList<IHostEvents::ClientSettingsChangedFn> ClientSettingsChanged;
    CallbackList<IHostEvents::ConsoleCommandFn> ConsoleCommand;
    CallbackList<IHostEvents::CheckTransmitFn> CheckTransmit;
    CallbackList<IHostEvents::BuildGameSessionManifestFn> BuildGameSessionManifest;

    ServiceTable Services;
    CommandNames Commands;
    LanguageTable Languages;
};

/** What a plugin had not released when it was removed. The host drops each and warns about it. */
struct Unreleased
{
    std::vector<std::string_view> Subscriptions;  ///< the event each was taken on
    std::vector<std::string> Services;

    bool Any() const;
};

/** One plugin's view of the host and the record of what it took, so every call has a known owner.
 *  Owned by @ref PluginHost; valid from AddPlugin until RemovePlugin. */
class HostView final : public IHost, public IHostEvents, public IHostServices
{
public:
    HostView(HostState& state, std::string name, std::string logTag, std::string version, uint64_t order);

    HostView(const HostView&) = delete;
    HostView& operator=(const HostView&) = delete;

    std::string_view PluginName() const { return _name; }

    void SetMinLogLevel(LogLevel level) { _minLevel = level; }

    /** Remove what the plugin still holds. Command names are the host's to remove, so they are not reported. */
    Unreleased RemoveAll();

    std::string_view Name() const override;
    std::string_view Version() const override;
    void* EngineInterface(const char* version) const override;
    void* ServerInterface(const char* version) const override;
    std::string_view BaseDir() const override;
    KHook::IKHook* HookDispatcher() const override;
    IHostEvents& Events() override;
    IHostServices& Services() override;
    IHostLanguages& Languages() override;
    IHostGameData* GameData() const override;
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

    void Publish(std::string_view name, void* implementation) override;
    void Unpublish(std::string_view name) override;
    void* Find(std::string_view name) override;
    uint64_t OnChanged(ChangedFn callback, void* context) override;

    /** Raise on this plugin alone what it missed by loading mid-map: OnServerStartup for the running
     *  map, then OnClientConnected and OnClientFullyConnected for each player already in. */
    void ReplayMissedEvents();
    /** Raise OnClientDisconnected on this plugin alone for each player still in, before a mid-map unload. */
    void DisconnectClients();

    /** Overrides both interfaces' Unsubscribe: there is one token space, and a token this plugin
     *  never took is ignored. */
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

    HostState& _state;
    std::string _name;
    std::string _logTag;
    std::string _version;
    LogLevel _minLevel = LogLevel::Info;
    uint64_t _order = 0;
    std::vector<Subscribed> _subscriptions;  ///< in the order the plugin took them
};

}  // namespace VoltMod
