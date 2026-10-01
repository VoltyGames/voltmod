#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Slots/SlotEvents.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Messaging/Messages.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace VoltMod
{

/**
 * @brief Re-sends a center-HTML panel until stopped, since CS2 drops center HTML on death, a team
 * switch and HUD updates. `render` runs on every send, so live values stay current.
 */
class CenterHtml
{
public:
    /** @p slots stops a panel when its player leaves. All must outlive this. */
    CenterHtml(Messages& messages, Scheduler& scheduler, SlotEvents& slots);

    /** Send `render(slot)` to @p slot every @p refreshMs, replacing any panel shown. */
    void Show(int slot, int refreshMs, std::function<std::string(int slot)> render);

    /** Stop and clear the panel; safe when nothing is shown. */
    void Stop(int slot);

    void StopAll();

private:
    Messages& _messages;
    Scheduler& _scheduler;
    std::array<Subscription, MaxPlayers> _timers;
    /** Declared last: unregisters before the timers it resets. */
    Subscription _slotListener;
};

}  // namespace VoltMod
