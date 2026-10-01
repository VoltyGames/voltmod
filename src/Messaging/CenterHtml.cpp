#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Messaging/CenterHtml.hpp>
#include <VoltMod/Messaging/Messages.hpp>
#include <utility>

namespace VoltMod
{

CenterHtml::CenterHtml(Messages& messages, Scheduler& scheduler, SlotEvents& slots)
    : _messages(messages), _scheduler(scheduler)
{
    _slotListener = slots.Changed += [this](int slot) {
        if (IsValidSlot(slot))
        {
            _timers[slot].Reset();
        }
    };
}

void CenterHtml::Show(int slot, int refreshMs, std::function<std::string(int slot)> render)
{
    if (!IsValidSlot(slot) || !render || refreshMs <= 0)
    {
        return;
    }

    Stop(slot);

    // _timers cancels it before `this` goes.
    auto send = [this, slot, render = std::move(render)]() { _messages.SendCenterHtml(slot, render(slot)); };
    send();
    _timers[slot] = _scheduler.Repeat(refreshMs, send);
}

void CenterHtml::Stop(int slot)
{
    if (!IsValidSlot(slot) || !_timers[slot])
    {
        return;
    }
    _timers[slot].Reset();
    _messages.ClearCenterHtml(slot);
}

void CenterHtml::StopAll()
{
    for (int slot = 0; slot < MaxPlayers; ++slot)
    {
        Stop(slot);
    }
}

}  // namespace VoltMod
