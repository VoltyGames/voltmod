#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/Server/Clock.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <cstdint>
#include <optional>

namespace VoltMod
{

/** Why a round ended, as the engine's `CSRoundEndReason` numbers it; `round_end` reports the same. */
enum class RoundEndReason : uint32_t
{
    CounterTerroristsWin = 8,
    TerroristsWin = 9,
    Draw = 10,
};

/**
 * @brief `runtime.Rounds`: the running round's timer, and ending the round with the engine's own win
 * panel and `round_end`, even with `mp_ignore_round_win_conditions` on. Team scores are not changed.
 *
 * @code
 * runtime.Rounds.SetTime(45 * 60);
 * runtime.Rounds.End(VoltMod::RoundEndReason::TerroristsWin, 5.0f);
 * @endcode
 *
 * Game-thread only.
 */
class Rounds
{
public:
    /** All three must outlive this service. */
    Rounds(EntitySystem& entities, const Bindings& bindings, const Clock& clock)
        : _entities(entities), _bindings(bindings), _clock(clock)
    {}
    Rounds(const Rounds&) = delete;
    Rounds& operator=(const Rounds&) = delete;

    /** Whether @ref End works: unsupported when the TerminateRound signature did not bind. */
    Status Available() const;

    /** End the round for @p reason; the next one starts after @p delaySeconds.
     *  @return Error::NotReady when no map is running. */
    Status End(RoundEndReason reason, float delaySeconds) const;

    /** Sets the running round to last @p seconds from its start, whatever `mp_roundtime` says; the
     *  round timer shows it at once. The next round starts from `mp_roundtime` again.
     *  @return Error::NotReady when no map is running. */
    Status SetTime(int seconds) const;

    /** Seconds until the round timer runs out, below 0 once it has; empty when no map is running. */
    std::optional<float> TimeLeft() const;

private:
    EntitySystem& _entities;
    const Bindings& _bindings;
    const Clock& _clock;
};

}  // namespace VoltMod
