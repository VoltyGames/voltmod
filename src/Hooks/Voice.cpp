#include <VoltMod/Core/Signals/HookResult.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Hooks/Voice.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <engine2/ivengineserver2.h>

namespace VoltMod
{

static HookResult<bool> FilterListening(Event<VoiceListen&>& before, IVEngineServer2& engine, CPlayerSlot receiver,
                                        CPlayerSlot sender, bool listen)
{
    if (!listen)
    {
        return {};
    }
    VoiceListen decision{.Receiver = receiver.Get(), .Sender = sender.Get()};
    before.Raise(decision);
    if (!decision.Blocked)
    {
        return {};
    }
    // The channel stays shut only once the engine's own implementation has been told.
    CallOriginal(&IVEngineServer2::SetClientListening, &engine, receiver, sender, false);
    return HookResult<bool>::Block(false);
}

Voice::Voice(Interfaces& interfaces, PlayerManager& players)
    : _hook("Voice", [this] { return Install(); }),
      Before(_hook.MakeLifecycle()),
      _interfaces(interfaces),
      _players(players)
{}

Result<Subscription> Voice::Install()
{
    if (auto available = Available(); !available)
    {
        return std::unexpected(available.error());
    }
    const auto onSetListening = [this](IVEngineServer2& engine, CPlayerSlot receiver, CPlayerSlot sender,
                                       bool listen) { return FilterListening(Before, engine, receiver, sender, listen); };
    return HookInterface(&IVEngineServer2::SetClientListening, _interfaces.Engine, onSetListening);
}

Status Voice::Available() const
{
    if (!_interfaces.Engine)
    {
        return std::unexpected(Error::NotReady("IVEngineServer2 not available"));
    }
    return {};
}

void Voice::Refresh(int senderSlot)
{
    IVEngineServer2* engine = _interfaces.Engine;
    if (!engine || !IsValidSlot(senderSlot))
    {
        return;
    }
    // Through the interface, so the hook runs and Before decides each pair again.
    for (const Player* receiver : _players.All())
    {
        if (receiver->Slot() != senderSlot)
        {
            engine->SetClientListening(CPlayerSlot(receiver->Slot()), CPlayerSlot(senderSlot), true);
        }
    }
}

}  // namespace VoltMod
