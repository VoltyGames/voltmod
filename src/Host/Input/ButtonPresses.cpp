#include "Host/Input/ButtonPresses.hpp"

#include "Engine/Net/MessageTypes.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Time/Durations.hpp>
#include <VoltMod/Engine/Detours.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <cstrike15_usermessages.pb.h>
#include <engine2/serversideclient.h>
#include <format>
#include <netmessages.pb.h>
#include <networksystem/inetchannel.h>
#include <networksystem/inetworkmessages.h>
#include <string_view>
#include <utility>

namespace VoltMod
{

// A press arrives as a CustomHudClicked body inside the generic user message.
static constexpr std::string_view UserMessageName = "CSVCMsg_UserMessage";
static constexpr int64_t WarningIntervalSeconds = 10;

ButtonPresses::ButtonPresses(const Bindings& bindings, const EngineInterfaces& engine, PluginRegistry& registry)
    : _registry(registry), _bindings(bindings), _warnings(WarningIntervalSeconds)
{
    if (!engine.NetworkMessages)
    {
        return;
    }
    if (Status installed = Install(engine); !installed)
    {
        Log::Warn("Button presses: {}; presses will not arrive.", installed.error().Detail);
    }
}

Status ButtonPresses::Install(const EngineInterfaces& engine)
{
    if (const auto* type = FindMessageType(*engine.NetworkMessages, UserMessageName))
    {
        _messageId = ReadMessageId(*type);
    }
    if (_messageId < 0)
    {
        return std::unexpected(Error::Engine(std::format("the engine does not know {}", UserMessageName)));
    }

    const auto onMessage = [this](INetworkMessageProcessingPreFilter& filter, const CNetMessage* message,
                                  INetChannel*) { Queue(message, filter); };
    auto hook = HookTable("Custom HUD button presses", &INetworkMessageProcessingPreFilter::FilterMessage,
                          _bindings.ClientFilterTable, onMessage);
    if (!hook)
    {
        return std::unexpected(hook.error());
    }
    _hook = std::move(*hook);
    return {};
}

void ButtonPresses::OnFrame()
{
    // Swapped out first: a handler may cause another press to queue.
    for (const Press& press : std::exchange(_queued, {}))
    {
        _registry.RaiseButtonPress(press.Slot, press.ButtonId);
    }
}

void ButtonPresses::Queue(const CNetMessage* message, const INetworkMessageProcessingPreFilter& filter)
{
    // Every inbound message passes here; the id rules out nearly all before any parsing.
    INetworkMessageInternal* info = message ? message->GetNetMessage() : nullptr;
    if (!info || ReadMessageId(*info) != _messageId)
    {
        return;
    }

    const auto* user = message->ToPB<CSVCMsg_UserMessage>();
    if (user->msg_type() != CS_UM_CustomHudClicked)
    {
        return;
    }

    // The sending connection's slot, so a spectator's press stays theirs.
    const int slot = static_cast<const CServerSideClientBase&>(filter).GetPlayerSlot().Get();
    if (!IsValidSlot(slot))
    {
        return;
    }

    // Parsed into our own message: the bytes come from the client, not the engine.
    CCSUsrMsg_CustomHudClicked click;
    if (!click.ParseFromString(user->msg_data()) || !click.has_button_id())
    {
        WarnMalformed(slot, "not a CustomHudClicked message with a button id");
        return;
    }
    if (click.button_id().find('\0') != std::string::npos)
    {
        return;
    }

    _queued.push_back({.Slot = slot, .ButtonId = click.button_id()});
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
