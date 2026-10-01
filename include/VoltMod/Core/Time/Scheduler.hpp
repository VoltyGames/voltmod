#pragma once

#include <VoltMod/Core/Signals/CallbackRegistry.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <cstdint>
#include <functional>

namespace VoltMod
{

/**
 * @brief One-shot delays and repeating timers, run on the game thread.
 *
 * Dropping a registration's @ref Subscription cancels its timer, one-shots included, so a callback
 * never outlives the state it captured.
 */
class Scheduler
{
public:
    Scheduler() = default;

    /** Run @p callback once after @p delayMs milliseconds. */
    [[nodiscard]] Subscription Delay(int64_t delayMs, std::function<void()> callback);

    /** Run @p callback every @p intervalMs milliseconds. */
    [[nodiscard]] Subscription Repeat(int64_t intervalMs, std::function<void()> callback);

    /** Run @p callback on the very next game frame. */
    [[nodiscard]] Subscription NextTick(std::function<void()> callback);

    /** Run @p callback every game frame. */
    [[nodiscard]] Subscription EveryFrame(std::function<void()> callback);

    /** Called every frame by the runtime. */
    void OnGameFrame();

private:
    struct Timer
    {
        int64_t NextFireTime;
        int64_t Interval;
        std::function<void()> Callback;
        uint64_t Id;
    };

    int64_t GetCurrentTimeMs() const;
    Subscription AddTimer(int64_t nextFireTime, int64_t interval, std::function<void()> callback);

    CallbackRegistry<Timer> _timers;
};

}  // namespace VoltMod
