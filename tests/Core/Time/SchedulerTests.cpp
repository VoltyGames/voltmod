#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <chrono>
#include <doctest/doctest.h>
#include <functional>
#include <thread>
#include <vector>

using VoltMod::Scheduler;
using VoltMod::Subscription;

TEST_CASE("EveryFrame with seconds runs once a frame")
{
    Scheduler scheduler;
    std::vector<float> calls;
    const Subscription frames = scheduler.EveryFrame([&](float seconds) { calls.push_back(seconds); });

    scheduler.OnGameFrame();
    scheduler.OnGameFrame();

    REQUIRE(calls.size() == 2);
    CHECK(calls[0] >= 0.0f);
    CHECK(calls[1] >= 0.0f);
}

TEST_CASE("EveryFrame with seconds caps a long frame")
{
    Scheduler scheduler;
    float last = -1.0f;
    const Subscription frames = scheduler.EveryFrame([&](float seconds) { last = seconds; }, 0.001f);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    scheduler.OnGameFrame();

    CHECK(last == doctest::Approx(0.001f));
}

TEST_CASE("EveryFrame with seconds stops when its subscription drops")
{
    Scheduler scheduler;
    int calls = 0;
    {
        const Subscription frames = scheduler.EveryFrame([&](float) { ++calls; });
        scheduler.OnGameFrame();
    }
    scheduler.OnGameFrame();

    CHECK(calls == 1);
}

TEST_CASE("EveryFrame with seconds ignores an empty callback")
{
    Scheduler scheduler;
    const Subscription frames = scheduler.EveryFrame(std::function<void(float)>{});

    CHECK_FALSE(static_cast<bool>(frames));
    scheduler.OnGameFrame();
}
