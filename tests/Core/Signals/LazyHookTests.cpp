#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/LazyHook.hpp>
#include <doctest/doctest.h>
#include <string>

using VoltMod::Error;
using VoltMod::Event;
using VoltMod::LazyHook;
using VoltMod::Result;
using VoltMod::Subscription;

// What a service sharing one engine hook across several events needs: install once when the first
// of them begins listening, remove once when the last one goes quiet.

TEST_CASE("The first handler installs the hook and the last one to drop removes it")
{
    int installs = 0;
    int removals = 0;
    LazyHook hook("Test", [&]() -> Result<Subscription> {
        ++installs;
        return Subscription([&] { ++removals; });
    });
    Event<int> event(hook.ForEvent());

    {
        auto first = event += [](int) {};
        auto second = event += [](int) {};
        CHECK(installs == 1);
        CHECK(removals == 0);
        CHECK(hook.Installed());
        // An Event reports only its empty-to-first and last-to-empty transitions, so two handlers
        // on one event are one subscriber here.
        CHECK(hook.ListeningEvents() == 1);
    }

    CHECK(removals == 1);
    CHECK_FALSE(hook.Installed());
    CHECK(hook.ListeningEvents() == 0);
}

TEST_CASE("One hook is shared across events with different handler signatures")
{
    int installs = 0;
    int removals = 0;
    LazyHook hook("Test", [&]() -> Result<Subscription> {
        ++installs;
        return Subscription([&] { ++removals; });
    });

    Event<int> pre(hook.ForEvent());
    Event<int, const std::string&> withPayload(hook.ForEvent());

    auto a = pre += [](int) {};
    auto b = withPayload += [](int, const std::string&) {};
    CHECK(installs == 1);
    CHECK(hook.ListeningEvents() == 2);

    b.Reset();
    CHECK(removals == 0);  // pre is still listening

    a.Reset();
    CHECK(removals == 1);
}

// Event's own Lifecycle refusal is covered in EventTests; what matters here is that a failed install
// leaves the hook with no listeners and is retried by whoever subscribes next.
TEST_CASE("A failed install counts no listener and is retried by the next subscriber")
{
    bool ready = false;
    int installs = 0;
    LazyHook hook("Test", [&]() -> Result<Subscription> {
        if (!ready)
        {
            return std::unexpected(Error::Unsupported("not bound"));
        }
        ++installs;
        return Subscription([] {});
    });
    Event<int> event(hook.ForEvent());

    {
        auto refused = event += [](int) {};
        CHECK_FALSE(static_cast<bool>(refused));
        CHECK(installs == 0);
        CHECK(hook.ListeningEvents() == 0);
    }

    ready = true;
    auto accepted = event += [](int) {};
    CHECK(static_cast<bool>(accepted));
    CHECK(installs == 1);
    CHECK(hook.ListeningEvents() == 1);
}
