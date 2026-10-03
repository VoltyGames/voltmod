#include "Host/Input/ButtonPresses.hpp"

#include "Engine/Net/ServerSideClients.hpp"
#include "Host/Input/ButtonPressMessage.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Time/Durations.hpp>
#include <VoltMod/Engine/Detours.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <inetchannel.h>
#include <networksystem/inetworkmessages.h>
#include <networksystem/netmessage.h>
#include <string_view>
#include <utility>

namespace VoltMod
{

// The SDK does not define CS_UM_CustomHudClicked, so decode it as a generic user message.
static constexpr std::string_view UserMessageName = "CSVCMsg_UserMessage";
static constexpr int32_t CustomHudClickType = 390;
static constexpr int64_t WarningIntervalSeconds = 10;

ButtonPresses::ButtonPresses(const Bindings& bindings, const EngineInterfaces& engine, PluginRegistry& registry)
    : _registry(registry), _bindings(bindings), _warnings(WarningIntervalSeconds)
{
    if (!engine.NetworkMessages)
    {
        return;
    }
    if (!_bindings.ClientMessageFilter)
    {
        Log::Warn("Button presses: the FilterMessage client offset did not bind; presses will not arrive.");
        return;
    }
    if (auto* message = engine.NetworkMessages->FindNetworkMessagePartial(std::string(UserMessageName).c_str()))
    {
        _messageId = message->GetNetMessageInfo()->m_MessageId;
    }
    if (_messageId < 0)
    {
        Log::Warn("Button presses: the engine does not know {}; presses will not arrive.", UserMessageName);
        return;
    }

    const auto onMessage = [this](INetworkMessageProcessingPreFilter& filter, const CNetMessage* message,
                                  INetChannel*) { Queue(message, filter); };
    auto hook = HookVirtual("Custom HUD button presses", _bindings.FilterMessage, onMessage);
    if (!hook)
    {
        Log::Warn("Button presses: {}; presses will not arrive.", hook.error().Detail);
        return;
    }
    _hook = std::move(*hook);
}

void ButtonPresses::OnFrame()
{
    // Swapped out first: a handler may cause another press to queue.
    for (const Press& press : std::exchange(_queued, {}))
    {
        _registry.RaiseButtonPress(press.Slot, press.ButtonId);
    }
}

const ButtonPresses::MessageFields& ButtonPresses::FieldsOf(const ProtoMessage& proto)
{
    static const MessageFields fields = [&proto] {
        const MessageFields resolved{.Type = ProtoField(proto, "msg_type"), .Data = ProtoField(proto, "msg_data")};
        if (!resolved)
        {
            Log::Warn("Button presses: {} has no 'msg_type'/'msg_data' field; ignoring presses.", UserMessageName);
        }
        return resolved;
    }();
    return fields;
}

void ButtonPresses::Queue(const CNetMessage* message, const INetworkMessageProcessingPreFilter& filter)
{
    // Every inbound message passes here; the id rules out nearly all before any parsing.
    INetworkMessageInternal* info = message ? message->GetNetMessage() : nullptr;
    if (!info || info->GetNetMessageInfo()->m_MessageId != _messageId)
    {
        return;
    }

    const ProtoMessage* proto = message->ToPB<ProtoMessage>();
    if (!proto)
    {
        return;
    }
    const MessageFields& fields = FieldsOf(*proto);
    if (!fields)
    {
        return;
    }

    const auto* reflection = proto->GetReflection();
    if (reflection->GetInt32(*proto, fields.Type) != CustomHudClickType)
    {
        return;
    }

    // The sending connection's slot, so a spectator's press stays theirs.
    const int slot = ClientSlot(_bindings, FilterClient(_bindings, filter));
    if (!IsValidSlot(slot))
    {
        return;
    }

    auto payload = ButtonPressMessage::Parse(reflection->GetString(*proto, fields.Data));
    if (!payload)
    {
        WarnMalformed(slot, payload.error().Detail);
        return;
    }
    if (payload->ButtonId.find('\0') != std::string::npos)
    {
        return;
    }

    _queued.push_back({.Slot = slot, .ButtonId = std::move(payload->ButtonId)});
}

void ButtonPresses::WarnMalformed(int slot, std::string_view detail)
{
    if (!_warnings.TryStart(slot, static_cast<int64_t>(Time::MonotonicSeconds())))
    {
        return;
    }
    Log::Warn("Button presses: a press from slot {} did not parse ({}).", slot, detail);
}

}  // namespace VoltMod
