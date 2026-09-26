#include <VoltMod/Entities/Entity.hpp>
#include <VoltMod/Entities/Rounds.hpp>
#include <VoltMod/Schema/Generated/CCSGameRulesProxy.hpp>

namespace VoltMod
{

// The game rules live outside the entity system; their proxy entity holds the pointer.
static Schema::CCSGameRules GameRules(EntitySystem& entities)
{
    const Entity proxy = entities.Find("cs_gamerules");
    return proxy ? Schema::CCSGameRulesProxy{proxy.Raw()}.GameRules() : Schema::CCSGameRules{};
}

static Status NoGameRules()
{
    return std::unexpected(Error::NotReady("no game rules: is a map running?"));
}

Status Rounds::Available() const
{
    if (!_bindings.TerminateRound)
    {
        return std::unexpected(Error::Unsupported("the CCSGameRules::TerminateRound signature did not bind"));
    }
    return {};
}

Status Rounds::End(RoundEndReason reason, float delaySeconds) const
{
    if (Status available = Available(); !available)
    {
        return available;
    }
    const Schema::CCSGameRules rules = GameRules(_entities);
    if (!rules)
    {
        return NoGameRules();
    }

    _bindings.TerminateRound(rules.Base(), delaySeconds, static_cast<uint32_t>(reason), nullptr);
    return {};
}

Status Rounds::SetTime(int seconds) const
{
    const Schema::CCSGameRules rules = GameRules(_entities);
    if (!rules)
    {
        return NoGameRules();
    }
    rules.SetRoundTime(seconds);
    return {};
}

std::optional<float> Rounds::TimeLeft() const
{
    const Schema::CCSGameRules rules = GameRules(_entities);
    if (!rules)
    {
        return std::nullopt;
    }
    return rules.RoundStartTime() + static_cast<float>(rules.RoundTime()) - _clock.Time();
}

}  // namespace VoltMod
