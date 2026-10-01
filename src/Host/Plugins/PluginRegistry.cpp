#include "Host/Plugins/PluginRegistry.hpp"

#include <algorithm>
#include <string>

namespace VoltMod
{

PluginRegistry::PluginRegistry(LoaderHandoff start, IPluginGameData* gameData, IPluginAddons* addons)
    : _state{.Start = start, .GameData = gameData, .Addons = addons}
{}

PluginRegistry::~PluginRegistry() = default;

void PluginRegistry::SetSchemaLayout(uint64_t stamp, bool verified)
{
    _state.SchemaLayoutStamp = stamp;
    _state.SchemaVerified = verified;
}

PluginContext* PluginRegistry::AddPlugin(std::string_view name, std::string_view logTag, std::string_view version)
{
    if (name.empty() || FindPlugin(name) != nullptr)
    {
        return nullptr;
    }

    _plugins.push_back(std::make_unique<PluginContext>(_state, std::string(name),
                                                       std::string(logTag.empty() ? name : logTag),
                                                       std::string(version), _state.NextOrder++));
    return _plugins.back().get();
}

PluginContext* PluginRegistry::FindPlugin(std::string_view name)
{
    const auto found = std::ranges::find_if(_plugins, [name](const auto& plugin) { return plugin->Name() == name; });
    return found != _plugins.end() ? found->get() : nullptr;
}

LeakReport PluginRegistry::RemovePlugin(std::string_view name)
{
    const auto found = std::ranges::find_if(_plugins, [name](const auto& plugin) { return plugin->Name() == name; });
    if (found == _plugins.end())
    {
        return {};
    }

    LeakReport leaks = (*found)->RemoveAll();
    _plugins.erase(found);
    return leaks;
}

void PluginRegistry::RaiseFrame()
{
    _state.Frame.Dispatch([](IPluginEvents::FrameFn callback, void* context) {
        callback(context);
        return false;
    });
}

void PluginRegistry::RaiseServerStartup(std::string_view mapName)
{
    _state.CurrentMap = mapName;
    _state.ServerStartup.Dispatch([&](IPluginEvents::ServerStartupFn callback, void* context) {
        callback(context, mapName);
        return false;
    });
}

std::string PluginRegistry::RaiseClientConnecting(int slot, int64_t steamId, std::string_view name)
{
    char reason[256] = {};
    const bool refused =
        _state.ClientConnecting.Dispatch([&](IPluginEvents::ClientConnectingFn callback, void* context) {
            return callback(context, slot, steamId, name, reason, sizeof reason);
        });
    if (!refused)
    {
        return {};
    }
    reason[sizeof reason - 1] = '\0';
    return reason[0] != '\0' ? std::string(reason) : std::string("Refused by the server.");
}

void PluginRegistry::RaiseClientConnected(int slot, int64_t steamId, std::string_view name, std::string_view address)
{
    // First, so a language a plugin restores on connect survives.
    _state.Languages.Reset(slot);
    if (IsValidSlot(slot))
    {
        _state.Clients[slot] =
            ConnectedClient{.SteamId = steamId, .Name = std::string(name), .Address = std::string(address)};
    }
    _state.ClientConnected.Dispatch([&](IPluginEvents::ClientConnectedFn callback, void* context) {
        callback(context, slot, steamId, name, address);
        return false;
    });
}

void PluginRegistry::RaiseClientDisconnected(int slot)
{
    _state.ClientDisconnected.Dispatch([&](IPluginEvents::ClientDisconnectedFn callback, void* context) {
        callback(context, slot);
        return false;
    });
    _state.Languages.Reset(slot);
    _state.Clients.Reset(slot);
}

void PluginRegistry::RaiseClientFullyConnected(int slot)
{
    if (IsValidSlot(slot) && _state.Clients[slot])
    {
        _state.Clients[slot]->FullyConnected = true;
    }
    _state.ClientFullyConnected.Dispatch([&](IPluginEvents::ClientFullyConnectedFn callback, void* context) {
        callback(context, slot);
        return false;
    });
}

void PluginRegistry::RaiseClientSettingsChanged(int slot)
{
    _state.ClientSettingsChanged.Dispatch([&](IPluginEvents::ClientSettingsChangedFn callback, void* context) {
        callback(context, slot);
        return false;
    });
}

bool PluginRegistry::RaiseConsoleCommand(std::string_view name, std::string_view arguments, int slot)
{
    return _state.ConsoleCommand.Dispatch([&](IPluginEvents::ConsoleCommandFn callback, void* context) {
        return callback(context, name, arguments, slot);
    });
}

void PluginRegistry::RaiseCheckTransmit(CCheckTransmitInfo** infoList, int infoCount)
{
    _state.CheckTransmit.Dispatch([&](IPluginEvents::CheckTransmitFn callback, void* context) {
        callback(context, infoList, infoCount);
        return false;
    });
}

void PluginRegistry::RaiseBuildGameSessionManifest(IEntityResourceManifest* manifest)
{
    _state.BuildGameSessionManifest.Dispatch([&](IPluginEvents::BuildGameSessionManifestFn callback, void* context) {
        callback(context, manifest);
        return false;
    });
}

bool PluginRegistry::HasCommandListeners() const
{
    return !_state.PlayerCommand.Empty() || !_state.PlayerCommandDone.Empty();
}

void PluginRegistry::RaisePlayerCommand(int slot, const PlayerInput& input)
{
    _state.PlayerCommand.Dispatch([&](IPluginEvents::PlayerCommandFn callback, void* context) {
        callback(context, slot, input);
        return false;
    });
}

void PluginRegistry::RaisePlayerCommandDone(int slot, const PlayerInput& input)
{
    _state.PlayerCommandDone.Dispatch([&](IPluginEvents::PlayerCommandFn callback, void* context) {
        callback(context, slot, input);
        return false;
    });
}

void PluginRegistry::RaiseButtonPress(int slot, std::string_view buttonId)
{
    _state.ButtonPress.Dispatch([&](IPluginEvents::ButtonPressFn callback, void* context) {
        callback(context, slot, buttonId);
        return false;
    });
}

void PluginRegistry::RegisterHostCommand(std::string_view name)
{
    _state.Commands.Reserve(name);
}

}  // namespace VoltMod
