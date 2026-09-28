#include "Commands/ArgBinding.hpp"
#include "Commands/Targeting.hpp"

#include <VoltMod/Core/Random.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>

namespace VoltMod
{

std::expected<std::vector<Player*>, TargetFailure> EngineArgBinder::Resolve(std::string_view token, Player* caller,
                                                                            const TargetRules& rules)
{
    if (token.empty())
    {
        return std::unexpected(TargetFailure{TargetError::NoMatch});
    }

    std::vector<PlayerView> candidates;
    candidates.reserve(_players.All().size());
    for (Player* player : _players.All())
    {
        const Pawn pawn = _entities.Pawn(player->Slot());
        candidates.push_back({
            .Slot = player->Slot(),
            .SteamId = player->SteamId(),
            .Name = player->Name(),
            .Team = pawn.TeamNum(),
            .Alive = pawn.IsAlive(),
            .Bot = player->IsBot(),
            // The command permission was already checked; this only asks whether the target may be acted on.
            .Targetable = !caller || _policy.Authorize(caller->Ref(), player->Ref(), {}).has_value(),
        });
    }

    const int callerSlot = caller ? caller->Slot() : -1;
    auto slots = FilterPlayers(candidates, ParseTargetToken(token), rules, callerSlot, RandomIndex);
    if (!slots)
    {
        return std::unexpected(slots.error());
    }

    std::vector<Player*> targets;
    targets.reserve(slots->size());
    for (int slot : *slots)
    {
        if (Player* player = _players.Get(slot))
        {
            targets.push_back(player);
        }
    }
    return targets;
}

}  // namespace VoltMod
