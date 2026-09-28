#pragma once

#include <VoltMod/Engine/Team.hpp>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/**
 * @brief The target-selector grammar behind a `Target()` command argument.
 *
 * Internal to command dispatch. Plugins select targets with an `Args::Target` handler parameter;
 * this implementation is not consumer API.
 *
 * Grammar: `@all`/`@*`, `@me`, `@!me`, `@t`, `@ct`, `@spec`, `@dead`, `@alive`, `@bot`,
 * `@human`, `@random`, `@randomt`, `@randomct`, `#slot`, a SteamID (64 / STEAM_ / [U:1:...]),
 * or a name fragment (exact match preferred, then prefix, then substring).
 *
 * @ref ParseTargetToken and @ref FilterPlayers operate on plain @ref PlayerView records, so the
 * grammar is unit-testable without a server.
 */

/** Which target classes a command permits for one Target argument. */
struct TargetRules
{
    bool AllowMultiple = false;  ///< permit selectors (@all/@t/...) that match more than one player
    bool AllowDead = true;
    bool AllowBots = true;
};

enum class TargetError
{
    NoMatch,
    Immune,           ///< matches existed, but the policy blocked all of them
    Ambiguous,        ///< a name fragment matched more than one player
    MultiNotAllowed,  ///< a multi-selector was used where the command takes a single target
    DeadNotAllowed,
    BotNotAllowed,
};

/** Why a resolution failed; Count carries the match count for Ambiguous messages. */
struct TargetFailure
{
    TargetError Error = TargetError::NoMatch;
    int Count = 0;
};

enum class TargetKind
{
    All,
    Me,
    NotMe,
    Team,  ///< uses TargetQuery::Team
    Dead,
    Alive,
    Bots,
    Humans,
    Random,
    RandomTeam,  ///< uses TargetQuery::Team
    Slot,
    SteamId,
    Name,
};

/** Parsed form of one target token. */
struct TargetQuery
{
    TargetKind Kind = TargetKind::Name;
    VoltMod::Team Team = VoltMod::Team::None;
    int Slot = -1;
    int64_t SteamId = 0;
    std::string Needle;  ///< lowercased name fragment for TargetKind::Name
};

TargetQuery ParseTargetToken(std::string_view token);

/** Engine-free snapshot of one connected player, for @ref FilterPlayers. */
struct PlayerView
{
    int Slot = -1;
    int64_t SteamId = 0;
    std::string Name;
    VoltMod::Team Team = VoltMod::Team::None;
    bool Alive = false;
    bool Bot = false;
    bool Targetable = true;  ///< policy verdict, precomputed by the caller
};

/** The slots of @p players that match @p query under @p rules; random kinds pick one through
 *  @p randomIndex(count). A failure says why the matches were rejected, so the caller can reply. */
std::expected<std::vector<int>, TargetFailure> FilterPlayers(
    std::span<const PlayerView> players, const TargetQuery& query, const TargetRules& rules, int callerSlot,
    const std::function<std::size_t(std::size_t)>& randomIndex = {});

}  // namespace VoltMod
