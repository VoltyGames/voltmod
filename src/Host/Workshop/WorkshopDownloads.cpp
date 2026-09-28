#include "Host/Workshop/WorkshopDownloads.hpp"

#include "Engine/Net/ServerSideClients.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Slots/SteamId.hpp>
#include <VoltMod/Core/Time/Durations.hpp>
#include <VoltMod/Engine/Detours.hpp>
#include <VoltMod/Engine/Memory/MemoryAccess.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <eiface.h>
#include <iserver.h>
#include <networkbasetypes.pb.h>
#include <networksystem/inetworkmessages.h>
#include <networksystem/netmessage.h>
#include <string>
#include <tier1/utlstring.h>
#include <utility>

namespace VoltMod
{

/** How soon a client must reconnect for its addon to count as downloaded. */
static constexpr double DownloadTimeoutSeconds = 30.0;
/** Offers of one addon before a declining client is dropped. */
static constexpr int MaxDownloadAttempts = 3;

WorkshopDownloads::WorkshopDownloads(GameDataTable* gameData, const EngineInterfaces& engine) : _engine(engine.Engine)
{
    if (gameData)
    {
        // Only four members matter here; the rest failing is the plugins' concern.
        (void)_bindings.Bind(
            [gameData](GameDataSection sections, std::string_view name) { return gameData->Lookup(sections, name); });
        _bound = true;
    }
}

uint64_t WorkshopDownloads::Add(uint64_t addonId)
{
    if (addonId == 0)
    {
        return 0;
    }
    if (Status hooked = InstallHooks(); !hooked)
    {
        Log::Warn("Addons: {} is not sent to clients: {}", addonId, hooked.error().Detail);
        return 0;
    }

    _queue.Add(addonId);
    const uint64_t token = _nextToken++;
    _tokens.emplace(token, addonId);
    return token;
}

void WorkshopDownloads::Remove(uint64_t token)
{
    const auto found = _tokens.find(token);
    if (found == _tokens.end())
    {
        return;
    }
    _queue.Remove(found->second);
    _tokens.erase(found);
    RemoveHooksIfUnused();
}

bool WorkshopDownloads::IsReady(int slot)
{
    if (_queue.Empty() || !IsValidSlot(slot) || !_engine)
    {
        return true;
    }
    return !_queue.HasMissing(static_cast<int64_t>(_engine->GetClientXUID(CPlayerSlot(slot))));
}

void WorkshopDownloads::OnClientConnected(int64_t steamId)
{
    if (!_queue.Empty())
    {
        _queue.RecordReconnect(steamId, Time::MonotonicSeconds(), DownloadTimeoutSeconds);
    }
}

void WorkshopDownloads::OnFrame()
{
    for (const Kick& kick : std::exchange(_kicks, {}))
    {
        // The slot may have changed hands since.
        if (static_cast<int64_t>(_engine->GetClientXUID(CPlayerSlot(kick.Slot))) == kick.SteamId)
        {
            _engine->DisconnectClient(CPlayerSlot(kick.Slot), NETWORK_DISCONNECT_TIMEDOUT,
                                      "Required workshop addon download was declined");
        }
    }
}

Status WorkshopDownloads::InstallHooks()
{
    if (_joinMessageHook)
    {
        return {};
    }

    // A listen server host needs no download step.
    if (!_engine || !_engine->IsDedicatedServer())
    {
        return std::unexpected(Error::Unsupported("addon downloads need a dedicated server"));
    }
    if (!_bound || !_bindings.ClientSteamId || !_bindings.ServerAddons)
    {
        return std::unexpected(Error::Unsupported("the client SteamID or server addons offset did not bind"));
    }

    auto join = HookVirtual("Workshop addon download", _bindings.SendNetMessage,
                            [this](EngineClient& client, const CNetMessage* message, NetChannelBufType_t) {
                                OnJoinMessage(message, &client);
                            });
    if (!join)
    {
        return std::unexpected(Error::Unsupported(join.error().Detail));
    }

    auto reply = HookFunction(
        "Workshop addon mount", _bindings.ReplyConnection,
        [this](CNetworkGameServerBase& server, EngineClient* client) { AddToReply(server, client); },
        [this](CNetworkGameServerBase& server, EngineClient*) { RestoreReply(server); });
    if (!reply)
    {
        return std::unexpected(Error::Unsupported(reply.error().Detail));
    }

    _joinMessageHook = std::move(*join);
    _connectionReplyHook = std::move(*reply);
    return {};
}

void WorkshopDownloads::RemoveHooksIfUnused()
{
    if (!_queue.Empty())
    {
        return;
    }

    _kicks.clear();
    _joinMessageHook.Reset();
    _connectionReplyHook.Reset();
    _addedToReply.clear();
    _queue.ClearProgress();
}

/** The server's addon list, or nullptr when the offset misses the string GetAddonName returns. */
static CUtlString* AddonList(const Bindings& bindings, CNetworkGameServerBase& server)
{
    auto* list = MemberPtr<CUtlString>(&server, bindings.ServerAddons.Value());
    // An empty list has no buffer to compare, and each side then returns its own "".
    const char* stored = list->Get();
    const char* reported = server.GetAddonName();
    const bool same = stored == reported || (*stored == '\0' && *reported == '\0');
    if (!same)
    {
        Log::Error("Addons: the CNetworkGameServer::m_szAddons offset {} is stale; not mounting addons.",
                   bindings.ServerAddons.Value());
        return nullptr;
    }
    return list;
}

void WorkshopDownloads::AddToReply(CNetworkGameServerBase& server, const EngineClient* client)
{
    const int64_t steamId = _bindings.ClientSteamId.Read(client);
    if (!SteamId::IsValid(steamId))
    {
        return;
    }

    const std::vector<uint64_t> mountList = _queue.ClientMountList(steamId);
    if (mountList.empty())
    {
        return;
    }

    // The client mounts only what the connection reply names.
    CUtlString* list = AddonList(_bindings, server);
    if (!list)
    {
        return;
    }
    std::string field = list->Get();
    _addedToReply = AppendToAddonList(field, mountList);
    if (_addedToReply.empty())
    {
        return;
    }

    list->Set(field.c_str());
    Log::Info("Addons: telling {} to mount {}.", steamId, field);
}

void WorkshopDownloads::RestoreReply(CNetworkGameServerBase& server)
{
    if (_addedToReply.empty())
    {
        return;
    }

    // Only our entries; the map's stay.
    CUtlString* list = AddonList(_bindings, server);
    if (!list)
    {
        _addedToReply.clear();
        return;
    }
    std::string field = list->Get();
    RemoveFromAddonList(field, _addedToReply);
    list->Set(field.c_str());
    _addedToReply.clear();
}

void WorkshopDownloads::OnJoinMessage(const CNetMessage* message, void* client)
{
    INetworkMessageInternal* info = message ? message->GetNetMessage() : nullptr;
    if (!info || info->GetNetMessageInfo()->m_MessageId != net_SignonState)
    {
        return;
    }

    const int64_t steamId = _bindings.ClientSteamId.Read(client);
    if (!SteamId::IsValid(steamId))
    {
        return;
    }

    // Rewritten in place, before the engine serializes it.
    auto* joinMessage = const_cast<CNetMessage*>(message)->ToPB<CNETMsg_SignonState>();
    const bool reconnect = joinMessage->signon_state() == SIGNONSTATE_CHANGELEVEL;
    const AddonDecision decision = _queue.DecideJoinMessage(steamId, reconnect, joinMessage->addons(),
                                                            Time::MonotonicSeconds(), MaxDownloadAttempts);

    switch (decision.Action)
    {
    case AddonAction::Unchanged:
        return;
    case AddonAction::TrimToFirst:
        Log::Info("Addons: a reconnect message named {} addons; sending {} and holding the rest.",
                  decision.Remaining + 1, decision.Id);
        [[fallthrough]];
    case AddonAction::KeepMounted:
        joinMessage->set_addons(std::to_string(decision.Id));
        return;
    case AddonAction::Kick:
        Log::Warn("Addons: {} did not take addon {} in {} attempts; dropping the client.", steamId, decision.Id,
                  MaxDownloadAttempts);
        if (const int slot = ClientSlot(_bindings, client); IsValidSlot(slot))
        {
            _kicks.push_back({.Slot = slot, .SteamId = steamId});
        }
        return;
    case AddonAction::Send:
        joinMessage->set_addons(std::to_string(decision.Id));
        joinMessage->set_signon_state(SIGNONSTATE_CHANGELEVEL);
        Log::Info("Addons: sending addon {} to {} ({} left after it).", decision.Id, steamId, decision.Remaining);
        return;
    }
}

}  // namespace VoltMod
