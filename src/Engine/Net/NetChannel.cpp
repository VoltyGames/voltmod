#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <VoltMod/Engine/Net/NetChannel.hpp>
#include <engine2/isource2server.h>
#include <inetchannelinfo.h>
#include <string>

namespace VoltMod
{

INetChannelInfo* NetChannels::GetNetInfo(int slot) const
{
    auto* engine = IsValidSlot(slot) ? _interfaces.Engine : nullptr;
    return engine ? engine->GetPlayerNetInfo(CPlayerSlot(slot)) : nullptr;
}

float NetChannels::EngineLatency(int slot) const
{
    auto* info = GetNetInfo(slot);
    return info ? info->GetEngineLatency() : 0.0f;
}

int NetChannels::GetUserId(int slot) const
{
    auto* engine = IsValidSlot(slot) ? _interfaces.Engine : nullptr;
    if (!engine)
    {
        return -1;
    }
    // The engine keeps userids in 16 bits and answers an empty slot with all of them set.
    const int id = engine->GetPlayerUserId(CPlayerSlot(slot)).Get();
    return id == 0xFFFF ? -1 : id;
}

std::optional<CommandStatus> NetChannels::GetCommandStatus(int slot) const
{
    auto* clients = IsValidSlot(slot) ? _interfaces.ServerGameClients : nullptr;
    if (!clients || !clients->IsPlayerSlotOccupied(CPlayerSlot(slot)))
    {
        return std::nullopt;
    }

    PlayerCommandStatus_t status{};
    clients->GetPlayerCommandStatus(CPlayerSlot(slot), &status);
    return CommandStatus{.LastExecuted = status.m_nLastCmdNumberExecuted,
                         .LastReceived = status.m_nLastCmdNumberReceived,
                         .ReceiveMargin = status.m_flLastCmdRecvMargin,
                         .StarvedTicks = status.m_nStarvedCommandTicks,
                         .DiscardedTicks = status.m_nDiscardedCommandTicks,
                         .HasQueueStats = status.m_bHasCommandQueueStats};
}

std::string_view NetChannels::GetUserInfoCvar(int slot, std::string_view name) const
{
    auto* engine = IsValidSlot(slot) ? _interfaces.Engine : nullptr;
    if (!engine || name.empty())
    {
        return {};
    }

    const char* value = engine->GetClientConVarValue(CPlayerSlot(slot), std::string(name).c_str());
    return value ? std::string_view(value) : std::string_view{};
}

}  // namespace VoltMod
