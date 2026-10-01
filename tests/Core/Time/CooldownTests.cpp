#include <VoltMod/Core/Time/Cooldown.hpp>
#include <cstdint>
#include <doctest/doctest.h>

using VoltMod::Cooldown;
using VoltMod::PairCooldown;

TEST_CASE("Cooldown: the first start takes, and the next waits it out")
{
    Cooldown<int> cooldown(60);
    CHECK(cooldown.TryStart(3, 1000));
    CHECK(!cooldown.TryStart(3, 1030));
    CHECK(!cooldown.TryStart(3, 1059));
    CHECK(cooldown.TryStart(3, 1060));
}

TEST_CASE("Cooldown: slots are independent")
{
    Cooldown<int> cooldown(60);
    CHECK(cooldown.TryStart(1, 1000));
    CHECK(cooldown.TryStart(2, 1000));
    CHECK(!cooldown.TryStart(1, 1010));
}

TEST_CASE("Cooldown: Reset frees a slot at once")
{
    Cooldown<int> cooldown(60);
    CHECK(cooldown.TryStart(5, 1000));
    CHECK(!cooldown.TryStart(5, 1010));
    cooldown.Reset(5);
    CHECK(cooldown.TryStart(5, 1011));
}

TEST_CASE("Cooldown: SecondsLeft counts down to zero")
{
    Cooldown<int> cooldown(60);
    CHECK(cooldown.SecondsLeft(3, 1000) == 0);
    CHECK(cooldown.TryStart(3, 1000));
    CHECK(cooldown.SecondsLeft(3, 1000) == 60);
    CHECK(cooldown.SecondsLeft(3, 1030) == 30);
    CHECK(cooldown.SecondsLeft(3, 1060) == 0);
}

TEST_CASE("Cooldown: per-call seconds override the constructed ones")
{
    Cooldown<int> cooldown(60);
    CHECK(cooldown.TryStart(3, 1000, 10));
    CHECK(!cooldown.TryStart(3, 1005, 10));
    CHECK(cooldown.TryStart(3, 1010, 10));
}

TEST_CASE("Cooldown: zero seconds never blocks")
{
    Cooldown<int> cooldown(0);
    CHECK(cooldown.TryStart(3, 1000));
    CHECK(cooldown.TryStart(3, 1000));
    CHECK(cooldown.SecondsLeft(3, 1000) == 0);
}

TEST_CASE("Cooldown: a clock jumping back frees the key")
{
    Cooldown<int> cooldown(60);
    CHECK(cooldown.TryStart(3, 5000));
    CHECK(cooldown.SecondsLeft(3, 1000) == 0);
    CHECK(cooldown.TryStart(3, 1000));
}

TEST_CASE("Cooldown: RemoveExpired drops only old keys")
{
    Cooldown<int> cooldown(60);
    cooldown.Start(1, 1000);
    cooldown.Start(2, 1900);
    cooldown.RemoveExpired(2000, 500);
    CHECK(cooldown.SecondsLeft(1, 1900) == 0);
    CHECK(cooldown.SecondsLeft(2, 1900) == 60);
}

TEST_CASE("PairCooldown: each pair is limited on its own")
{
    PairCooldown<int64_t, int64_t> cooldown(1800);
    CHECK(cooldown.TryStart({7, 8}, 1000));
    CHECK(!cooldown.TryStart({7, 8}, 1500));
    CHECK(cooldown.TryStart({7, 9}, 1500));
    CHECK(cooldown.TryStart({9, 8}, 1500));
    CHECK(cooldown.TryStart({7, 8}, 2800));
}
