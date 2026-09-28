#include "Host/Plugins/PluginContext.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace VoltMod
{

PluginContext::PluginContext(SharedState& state, std::string name, std::string logTag, std::string version,
                             uint64_t order)
    : _state(state), _name(std::move(name)), _logTag(std::move(logTag)), _version(std::move(version)), _order(order)
{}

std::string_view PluginContext::Name() const
{
    return _name;
}

std::string_view PluginContext::Version() const
{
    return _version;
}

void* PluginContext::EngineInterface(const char* version) const
{
    return _state.Start.EngineFactory(version, nullptr);
}

void* PluginContext::ServerInterface(const char* version) const
{
    return _state.Start.ServerFactory(version, nullptr);
}

std::string_view PluginContext::BaseDir() const
{
    return _state.Start.GameDir;
}

KHook::IKHook* PluginContext::HookDispatcher() const
{
    return _state.Start.HookDispatcher;
}

IPluginEvents& PluginContext::Events()
{
    return *this;
}

IPluginServices& PluginContext::Services()
{
    return *this;
}

IPluginGameData* PluginContext::GameData() const
{
    return _state.GameData;
}

bool PluginContext::RegisterCommand(std::string_view name)
{
    if (_state.Commands.Register(this, _name, name))
    {
        return true;
    }

    Log::Error("Command '{}' is already registered by {}, so {} cannot have it.", name, _state.Commands.OwnerOf(name),
               _name);
    return false;
}

bool PluginContext::IsCommandRegistered(std::string_view name) const
{
    return !_state.Commands.OwnerOf(name).empty();
}

IPluginLanguages& PluginContext::Languages()
{
    return _state.Languages;
}

IPluginAddons& PluginContext::Addons()
{
    return *this;
}

bool PluginContext::Add(uint64_t addonId)
{
    if (!_state.Addons || !_state.Addons->Add(addonId))
    {
        return false;
    }
    _addons.push_back(addonId);
    return true;
}

void PluginContext::Remove(uint64_t addonId)
{
    const auto held = std::ranges::find(_addons, addonId);
    if (held != _addons.end())
    {
        _addons.erase(held);
        _state.Addons->Remove(addonId);
    }
}

bool PluginContext::IsReady(int slot)
{
    return !_state.Addons || _state.Addons->IsReady(slot);
}

void PluginContext::WriteLog(uint8_t level, std::string_view text)
{
    const auto wanted = static_cast<LogLevel>(level);
    if (wanted < _minLevel)
    {
        return;
    }

    // Every plugin prints through the host's own handler, so a server reads one stream.
    Log::Emit(wanted, std::format("[{}] {}", _logTag, text));
}

uint8_t PluginContext::MinLogLevel() const
{
    return static_cast<uint8_t>(_minLevel);
}

uint64_t PluginContext::SchemaLayoutStamp() const
{
    return _state.SchemaLayoutStamp;
}

bool PluginContext::SchemaVerified() const
{
    return _state.SchemaVerified;
}

template <class Fn>
uint64_t PluginContext::Subscribe(std::string_view event, CallbackList<Fn>& list, Fn callback, void* context)
{
    if (callback == nullptr)
    {
        return 0;
    }

    const uint64_t token = _state.NextToken++;
    list.Add(token, _order, callback, context);
    _subscriptions.push_back({.Token = token, .Event = event, .Remove = [&list, token] { list.Remove(token); }});
    return token;
}

uint64_t PluginContext::OnFrame(FrameFn callback, void* context)
{
    return Subscribe("frame", _state.Frame, callback, context);
}

uint64_t PluginContext::OnServerStartup(ServerStartupFn callback, void* context)
{
    return Subscribe("server startup", _state.ServerStartup, callback, context);
}

uint64_t PluginContext::OnClientConnecting(ClientConnectingFn callback, void* context)
{
    return Subscribe("client connecting", _state.ClientConnecting, callback, context);
}

uint64_t PluginContext::OnClientConnected(ClientConnectedFn callback, void* context)
{
    return Subscribe("client connected", _state.ClientConnected, callback, context);
}

uint64_t PluginContext::OnClientDisconnected(ClientDisconnectedFn callback, void* context)
{
    return Subscribe("client disconnected", _state.ClientDisconnected, callback, context);
}

uint64_t PluginContext::OnClientFullyConnected(ClientFullyConnectedFn callback, void* context)
{
    return Subscribe("client fully connected", _state.ClientFullyConnected, callback, context);
}

uint64_t PluginContext::OnClientSettingsChanged(ClientSettingsChangedFn callback, void* context)
{
    return Subscribe("client settings changed", _state.ClientSettingsChanged, callback, context);
}

uint64_t PluginContext::OnConsoleCommand(ConsoleCommandFn callback, void* context)
{
    return Subscribe("console command", _state.ConsoleCommand, callback, context);
}

uint64_t PluginContext::OnCheckTransmit(CheckTransmitFn callback, void* context)
{
    return Subscribe("check transmit", _state.CheckTransmit, callback, context);
}

uint64_t PluginContext::OnBuildGameSessionManifest(BuildGameSessionManifestFn callback, void* context)
{
    return Subscribe("build game session manifest", _state.BuildGameSessionManifest, callback, context);
}

void PluginContext::Publish(std::string_view name, void* implementation)
{
    _state.Services.Publish(this, name, implementation);
}

void PluginContext::Unpublish(std::string_view name)
{
    _state.Services.Unpublish(this, name);
}

void* PluginContext::Find(std::string_view name)
{
    return _state.Services.Find(name);
}

void PluginContext::ReplayMissedEvents()
{
    if (_state.CurrentMap.empty())
    {
        return;
    }

    _state.ServerStartup.DispatchTo(
        _order, [&](ServerStartupFn callback, void* context) { callback(context, _state.CurrentMap); });
    // Each slot checked before each event: a handler may kick, which empties it.
    for (int slot = 0; slot < MaxPlayers; ++slot)
    {
        if (!_state.Clients[slot])
        {
            continue;
        }
        const ConnectedClient client = *_state.Clients[slot];
        _state.ClientConnected.DispatchTo(_order, [&](ClientConnectedFn callback, void* context) {
            callback(context, slot, client.SteamId, client.Name, client.Address);
        });

        if (_state.Clients[slot] && _state.Clients[slot]->FullyConnected)
        {
            _state.ClientFullyConnected.DispatchTo(
                _order, [&](ClientFullyConnectedFn callback, void* context) { callback(context, slot); });
        }
    }
}

void PluginContext::DisconnectClients()
{
    for (int slot = 0; slot < MaxPlayers; ++slot)
    {
        if (_state.Clients[slot])
        {
            _state.ClientDisconnected.DispatchTo(
                _order, [&](ClientDisconnectedFn callback, void* context) { callback(context, slot); });
        }
    }
}

void PluginContext::Unsubscribe(uint64_t token)
{
    const auto held = std::ranges::find(_subscriptions, token, &Subscribed::Token);
    if (held == _subscriptions.end())
    {
        return;
    }

    held->Remove();
    _subscriptions.erase(held);
}

LeakReport PluginContext::RemoveAll()
{
    LeakReport leaks;
    // Subscriptions first, so the plugin on its way out is not told about its own withdrawals.
    for (const Subscribed& held : _subscriptions)
    {
        held.Remove();
        leaks.Subscriptions.push_back(held.Event);
    }
    _subscriptions.clear();

    leaks.Services = _state.Services.RemoveAll(this);
    for (const uint64_t addonId : std::exchange(_addons, {}))
    {
        _state.Addons->Remove(addonId);
        leaks.Addons.push_back(addonId);
    }
    _state.Commands.RemoveAll(this);
    return leaks;
}

}  // namespace VoltMod
