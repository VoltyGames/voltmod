#include <VoltMod/Core/Signals/CallbackRegistry.hpp>
#include <doctest/doctest.h>
#include <functional>
#include <vector>

using VoltMod::CallbackRegistry;
using VoltMod::Subscription;

using Fn = std::function<void()>;

TEST_CASE("Dispatch invokes every registered item")
{
    CallbackRegistry<Fn> registry;
    int calls = 0;
    auto a = registry.AddOwned([&] { ++calls; });
    auto b = registry.AddOwned([&] { ++calls; });

    registry.Dispatch([](Fn& fn) { fn(); });

    CHECK(calls == 2);
}

TEST_CASE("Dispatch on an empty registry does nothing")
{
    CallbackRegistry<Fn> registry;
    registry.Dispatch([](Fn& fn) { fn(); });
    CHECK(registry.Empty());
}

TEST_CASE("A callback may drop its own subscription while dispatching")
{
    CallbackRegistry<Fn> registry;
    Subscription self;
    int calls = 0;

    self = registry.AddOwned([&] {
        ++calls;
        self.Reset();  // erases this very entry mid-dispatch
    });

    registry.Dispatch([](Fn& fn) { fn(); });

    CHECK(calls == 1);
    CHECK(registry.Empty());
}

TEST_CASE("A callback that removes a later one stops it from running")
{
    CallbackRegistry<Fn> registry;
    Subscription second;
    int firstCalls = 0;
    int secondCalls = 0;

    auto first = registry.AddOwned([&] {
        ++firstCalls;
        second.Reset();
    });
    second = registry.AddOwned([&] { ++secondCalls; });

    registry.Dispatch([](Fn& fn) { fn(); });

    CHECK(firstCalls == 1);
    CHECK(secondCalls == 0);
    CHECK(registry.Size() == 1);
}

TEST_CASE("A callback registering during dispatch does not run in the same pass")
{
    CallbackRegistry<Fn> registry;
    std::vector<Subscription> held;
    int added = 0;
    int outer = 0;

    auto first = registry.AddOwned([&] {
        ++outer;
        held.push_back(registry.AddOwned([&] { ++added; }));
    });

    registry.Dispatch([](Fn& fn) { fn(); });

    CHECK(outer == 1);
    CHECK(added == 0);
}

TEST_CASE("An entry removed in a nested dispatch is gone once the outer one ends")
{
    CallbackRegistry<Fn> registry;
    Subscription inner;
    int innerCalls = 0;
    bool nested = false;

    auto outer = registry.AddOwned([&] {
        if (nested)
        {
            return;
        }
        nested = true;
        registry.Dispatch([](Fn& fn) { fn(); });
        inner.Reset();
    });
    inner = registry.AddOwned([&] { ++innerCalls; });

    registry.Dispatch([](Fn& fn) { fn(); });

    CHECK(innerCalls == 1);
    CHECK(registry.Size() == 1);
    CHECK(registry.Find(2) == nullptr);
}

TEST_CASE("Dispatch survives many entries")
{
    CallbackRegistry<Fn> registry;
    std::vector<Subscription> subs;
    int calls = 0;
    for (int i = 0; i < 20; ++i)
    {
        subs.push_back(registry.AddOwned([&] { ++calls; }));
    }

    registry.Dispatch([](Fn& fn) { fn(); });

    CHECK(calls == 20);
}
