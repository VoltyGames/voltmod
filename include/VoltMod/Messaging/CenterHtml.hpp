#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/PerSlot.hpp>
#include <VoltMod/Core/Slots/SlotEvents.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Messaging/Messages.hpp>
#include <cstdint>
#include <functional>
#include <string>

namespace VoltMod
{

/** How often an unchanged center-HTML page is sent again: CS2 drops it on death, a team switch and
 *  HUD updates. */
inline constexpr int64_t CenterHtmlKeepAliveMs = 250;

/**
 * @brief Shows a center-HTML panel until stopped. `render` runs every refresh, so live values stay
 * current; a page goes out when it changes and at least every @ref CenterHtmlKeepAliveMs.
 */
class CenterHtml
{
public:
    /** @p slots stops a panel when its player leaves. All must outlive this. */
    CenterHtml(Messages& messages, Scheduler& scheduler, SlotEvents& slots);

    /** Render for @p slot every @p refreshMs and send what changed, replacing any panel shown. */
    void Show(int slot, int refreshMs, std::function<std::string(int slot)> render);

    /** Stop and clear the panel; safe when nothing is shown. */
    void Stop(int slot);

    void StopAll();

private:
    Messages& _messages;
    Scheduler& _scheduler;
    PerSlot<Subscription> _timers;
};

}  // namespace VoltMod
