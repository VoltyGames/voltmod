#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Time/Durations.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Messaging/CenterHtml.hpp>
#include <VoltMod/Messaging/Messages.hpp>
#include <string>
#include <utility>

namespace VoltMod
{

CenterHtml::CenterHtml(Messages& messages, Scheduler& scheduler, SlotEvents& slots)
    : _messages(messages), _scheduler(scheduler), _timers(slots)
{}

void CenterHtml::Show(int slot, int refreshMs, std::function<std::string(int slot)> render)
{
    if (!IsValidSlot(slot) || !render || refreshMs <= 0)
    {
        return;
    }

    Stop(slot);

    // _timers cancels it before `this` goes.
    auto send = [this, slot, render = std::move(render), sent = std::string(), sentAt = int64_t{0}]() mutable {
        std::string html = render(slot);
        const int64_t now = Time::MonotonicMs();
        if (html == sent && now - sentAt < CenterHtmlKeepAliveMs)
        {
            return;
        }
        _messages.SendCenterHtml(slot, html);
        sent = std::move(html);
        sentAt = now;
    };
    send();
    _timers[slot] = _scheduler.Repeat(refreshMs, std::move(send));
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
