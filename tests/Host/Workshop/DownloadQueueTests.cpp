#include "Host/Workshop/DownloadQueue.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <vector>

using VoltMod::AddonAction;
using VoltMod::AppendToAddonList;
using VoltMod::DownloadQueue;
using VoltMod::ParseAddonList;
using VoltMod::RemoveFromAddonList;

using Ids = std::vector<uint64_t>;

static constexpr int64_t kPlayer = 76561198000000001LL;
static constexpr int64_t kOtherPlayer = 76561198000000002LL;
static constexpr int kMaxAttempts = 3;
static constexpr double kTimeout = 30.0;

/** Offer the next addon at @p now and reconnect a second later. Returns the addon offered. */
static uint64_t Download(DownloadQueue& downloads, double now)
{
    const uint64_t id = downloads.NextToSend(kPlayer, now, kMaxAttempts).Id;
    downloads.RecordReconnect(kPlayer, now + 1.0, kTimeout);
    return id;
}

TEST_CASE("An addon counts as downloaded only after a prompt reconnect")
{
    DownloadQueue downloads;
    downloads.Require(100);

    const auto decision = downloads.NextToSend(kPlayer, 10.0, kMaxAttempts);
    CHECK(decision.Action == AddonAction::Send);
    CHECK(decision.Id == 100);
    CHECK(downloads.MissingFor(kPlayer) == Ids{100});

    downloads.RecordReconnect(kPlayer, 100.0, kTimeout);
    CHECK(downloads.MissingFor(kPlayer) == Ids{100});

    Download(downloads, 200.0);
    CHECK(downloads.MissingFor(kPlayer).empty());
    CHECK(downloads.NextToSend(kPlayer, 300.0, kMaxAttempts).Action == AddonAction::Unchanged);
}

TEST_CASE("Addons go out one per reconnect, in the order they were required")
{
    DownloadQueue downloads;
    downloads.Require(100);
    downloads.Require(200);

    CHECK(downloads.Required() == Ids{100, 200});
    CHECK(downloads.MissingFor(kPlayer) == Ids{100, 200});

    CHECK(Download(downloads, 1.0) == 100);
    CHECK(Download(downloads, 3.0) == 200);
    CHECK_FALSE(downloads.HasMissing(kPlayer));
    CHECK(downloads.HasMissing(kOtherPlayer));
}

TEST_CASE("Offers past the attempt cap kick the client, and a prompt reconnect resets the count")
{
    DownloadQueue downloads;
    downloads.Require(100);
    downloads.Require(200);

    downloads.NextToSend(kPlayer, 1.0, kMaxAttempts);
    downloads.NextToSend(kPlayer, 2.0, kMaxAttempts);
    downloads.RecordReconnect(kPlayer, 3.0, kTimeout);

    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt)
    {
        CHECK(downloads.NextToSend(kPlayer, 3.0 + attempt, kMaxAttempts).Action == AddonAction::Send);
    }

    const auto decision = downloads.NextToSend(kPlayer, 10.0, kMaxAttempts);
    CHECK(decision.Action == AddonAction::Kick);
    CHECK(decision.Id == 200);
}

TEST_CASE("Requirements are reference counted, and id zero is refused")
{
    DownloadQueue downloads;
    CHECK_FALSE(downloads.Require(0));
    downloads.Release(100);

    downloads.Require(100);
    downloads.Require(100);

    downloads.Release(100);
    CHECK(downloads.Required() == Ids{100});

    downloads.Release(100);
    downloads.Release(100);
    CHECK(downloads.Empty());
}

TEST_CASE("An addon the engine is already sending costs no attempt and is not sent again")
{
    DownloadQueue downloads;
    downloads.Require(100);
    downloads.Require(200);

    for (int attempt = 0; attempt < 10; ++attempt)
    {
        downloads.MarkSending(kPlayer, 100, attempt);
    }
    CHECK(downloads.NextToSend(kPlayer, 10.0, kMaxAttempts).Action == AddonAction::Send);

    downloads.RecordReconnect(kPlayer, 11.0, kTimeout);
    CHECK(downloads.NextToSend(kPlayer, 12.0, kMaxAttempts).Id == 200);
}

TEST_CASE("Clearing progress keeps the requirements")
{
    DownloadQueue downloads;
    downloads.Require(100);
    Download(downloads, 1.0);
    CHECK(downloads.MissingFor(kPlayer).empty());

    downloads.ClearProgress();
    CHECK(downloads.MissingFor(kPlayer) == Ids{100});
    CHECK(downloads.MissingFor(kOtherPlayer) == Ids{100});
}

TEST_CASE("A reconnect message keeps only its first addon, and counts it as sending")
{
    DownloadQueue downloads;
    downloads.Require(100);

    CHECK(downloads.DecideJoinMessage(kPlayer, true, "", 1.0, kMaxAttempts).Action == AddonAction::Unchanged);
    downloads.RecordReconnect(kPlayer, 2.0, kTimeout);
    CHECK(downloads.MissingFor(kPlayer) == Ids{100});

    const auto decision = downloads.DecideJoinMessage(kPlayer, true, "100,200", 3.0, kMaxAttempts);
    CHECK(decision.Action == AddonAction::TrimToFirst);
    CHECK(decision.Id == 100);
    CHECK(decision.Remaining == 1);

    downloads.RecordReconnect(kPlayer, 4.0, kTimeout);
    CHECK(downloads.MissingFor(kPlayer).empty());
}

TEST_CASE("A map change message names the addon the client already downloaded")
{
    DownloadQueue downloads;
    downloads.Require(100);

    downloads.DecideJoinMessage(kPlayer, false, "", 1.0, kMaxAttempts);
    downloads.RecordReconnect(kPlayer, 2.0, kTimeout);

    const auto decision = downloads.DecideJoinMessage(kPlayer, true, "", 3.0, kMaxAttempts);
    CHECK(decision.Action == AddonAction::KeepMounted);
    CHECK(decision.Id == 100);
}

TEST_CASE("An addons field parses as a comma separated list, skipping malformed entries")
{
    CHECK(ParseAddonList("").empty());
    CHECK(ParseAddonList("100,200,300") == Ids{100, 200, 300});
    CHECK(ParseAddonList("100,,abc,200") == Ids{100, 200});
    CHECK(ParseAddonList("100x").empty());
    CHECK(ParseAddonList("0").empty());
}

TEST_CASE("A client mounts the required addons it downloaded or is downloading")
{
    DownloadQueue downloads;
    downloads.Require(100);
    downloads.Require(200);
    CHECK(downloads.ClientMountList(kPlayer).empty());

    Download(downloads, 1.0);
    downloads.NextToSend(kPlayer, 3.0, kMaxAttempts);
    CHECK(downloads.ClientMountList(kPlayer) == Ids{100, 200});
    CHECK(downloads.ClientMountList(kOtherPlayer).empty());

    downloads.Release(100);
    CHECK(downloads.ClientMountList(kPlayer) == Ids{200});
}

TEST_CASE("Reply edits name each addon once and take back only their own, in either order")
{
    std::string single;
    CHECK(AppendToAddonList(single, {100}) == Ids{100});
    CHECK(single == "100");
    RemoveFromAddonList(single, {100, 200});
    CHECK(single.empty());

    for (const bool firstRestoresFirst : {true, false})
    {
        std::string field = "5000";
        const Ids first = AppendToAddonList(field, {100, 5000, 100});
        const Ids second = AppendToAddonList(field, {100, 200});
        CHECK(first == Ids{100});
        CHECK(second == Ids{200});
        CHECK(field == "5000,100,200");

        RemoveFromAddonList(field, firstRestoresFirst ? first : second);
        CHECK(field == (firstRestoresFirst ? "5000,200" : "5000,100"));
        RemoveFromAddonList(field, firstRestoresFirst ? second : first);
        CHECK(field == "5000");
    }
}
