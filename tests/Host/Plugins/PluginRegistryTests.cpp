#include "Host/Plugins/PluginRegistry.hpp"

#include <algorithm>
#include <doctest/doctest.h>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

using VoltMod::LeakReport;
using VoltMod::PluginContext;
using VoltMod::PluginRegistry;

/** How many frame callbacks ran, the cheapest proof that a subscription is still live. */
struct CoreCounter
{
    int Calls = 0;
};

static void CountFrame(void* context)
{
    ++static_cast<CoreCounter*>(context)->Calls;
}

TEST_CASE("A view answers for the plugin it was added for, and a name is added once")
{
    PluginRegistry host;
    PluginContext* plugin = host.AddPlugin("bhop");
    REQUIRE(plugin != nullptr);

    CHECK(plugin->Name() == "bhop");
    CHECK(host.FindPlugin("bhop") == plugin);
    CHECK(host.AddPlugin("bhop") == nullptr);
}

TEST_CASE("Tokens are unique across every event and are never reused")
{
    PluginRegistry host;
    PluginContext* first = host.AddPlugin("first");
    PluginContext* second = host.AddPlugin("second");

    CoreCounter counter;
    std::vector<uint64_t> tokens{
        first->OnFrame(CountFrame, &counter),
        first->OnConsoleCommand(
            +[](void*, std::string_view, std::string_view, int) { return false; }, &counter),
        second->OnFrame(CountFrame, &counter),
    };

    for (const uint64_t token : tokens)
    {
        CHECK(token != 0);
    }
    std::vector<uint64_t> sorted = tokens;
    std::ranges::sort(sorted);
    CHECK(std::ranges::adjacent_find(sorted) == sorted.end());

    first->Unsubscribe(tokens.front());
    const uint64_t reissued = first->OnFrame(CountFrame, &counter);
    CHECK(std::ranges::find(tokens, reissued) == tokens.end());
}

TEST_CASE("Unsubscribing a token another plugin took does nothing")
{
    PluginRegistry host;
    PluginContext* first = host.AddPlugin("first");
    PluginContext* second = host.AddPlugin("second");

    CoreCounter counter;
    const uint64_t token = first->OnFrame(CountFrame, &counter);

    second->Unsubscribe(token);
    host.RaiseFrame();

    CHECK(counter.Calls == 1);
}

TEST_CASE("A second plugin registering a command name fails")
{
    PluginRegistry host;
    PluginContext* first = host.AddPlugin("first");
    PluginContext* second = host.AddPlugin("second");

    CHECK(first->RegisterCommand("ban"));
    CHECK_FALSE(second->RegisterCommand("ban"));
    CHECK(first->RegisterCommand("ban"));  // its own name again is no conflict
}

TEST_CASE("Removing a plugin drops what it still held and reports each leftover")
{
    PluginRegistry host;
    PluginContext* first = host.AddPlugin("first");
    PluginContext* second = host.AddPlugin("second");

    CoreCounter counter;
    first->OnFrame(CountFrame, &counter);
    first->OnClientDisconnected(+[](void*, int) {}, &counter);
    int implementation = 7;
    first->Publish("first.api", &implementation);
    CHECK(first->RegisterCommand("ban"));
    second->OnFrame(CountFrame, &counter);

    host.RaiseFrame();
    CHECK(counter.Calls == 2);

    const LeakReport unreleased = host.RemovePlugin("first");

    CHECK(unreleased.Subscriptions == std::vector<std::string_view>{"frame", "client_disconnected"});
    CHECK(unreleased.Services == std::vector<std::string>{"first.api"});

    counter.Calls = 0;
    host.RaiseFrame();
    CHECK(counter.Calls == 1);
    CHECK(second->Find("first.api") == nullptr);
    CHECK(second->RegisterCommand("ban"));
    CHECK(host.FindPlugin("first") == nullptr);
}

TEST_CASE("Removing a plugin that held nothing reports no leak")
{
    PluginRegistry host;
    host.AddPlugin("first");

    const LeakReport unreleased = host.RemovePlugin("first");

    CHECK(unreleased.Subscriptions.empty());
    CHECK(unreleased.Services.empty());
    CHECK(host.RemovePlugin("never-loaded").Services.empty());
}

TEST_CASE("A language one plugin sets reaches the others until the slot changes hands")
{
    PluginRegistry host;
    PluginContext* picker = host.AddPlugin("picker");
    PluginContext* reader = host.AddPlugin("reader");
    REQUIRE(picker != nullptr);
    REQUIRE(reader != nullptr);

    picker->Languages().SetLanguage(2, "ru");
    CHECK(reader->Languages().Language(2) == "ru");

    host.RaiseClientDisconnected(2);
    CHECK(reader->Languages().Language(2).empty());

    picker->Languages().SetLanguage(2, "ru");
    host.RaiseClientConnected(2, 76561198000000000LL, "next", "10.0.0.3");
    CHECK(reader->Languages().Language(2).empty());
}
