#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Entities/KeyValues.hpp>
#include <VoltMod/Hooks/GlowVision.hpp>
#include <VoltMod/Hooks/Visibility.hpp>
#include <VoltMod/Schema/Generated/Enums.hpp>
#include <utility>

namespace VoltMod
{

static constexpr int PropSpawnFlags = 256;

void GlowVision::DestroyPair(GlowPair& pair)
{
    if (!pair.Active())
    {
        return;
    }

    _visibility.ShowToEveryone(pair.Relay);
    _visibility.ShowToEveryone(pair.Glow);

    _entities.Get(pair.Glow).Remove();
    _entities.Get(pair.Relay).Remove();

    pair = {};
}

void GlowVision::CreatePair(int slot, GlowPair& pair)
{
    Pawn pawn = _entities.Pawn(slot);
    if (!pawn)
    {
        return;
    }

    std::string model = pawn.ModelName();
    const Team team = pawn.TeamNum();
    if (model.empty())
    {
        return;
    }

    KeyValues relayKv;
    relayKv.Set("model", model.c_str())
        .Set("spawnflags", PropSpawnFlags)
        .Set("rendermode", static_cast<int>(Schema::RenderMode_t::kRenderNone));
    const Entity relay = _entities.Spawn("prop_dynamic", relayKv);
    if (!relay)
    {
        return;
    }

    relay.AcceptInput("FollowEntity", "!activator", pawn);
    const Entity glow = _entities.SpawnOutline(
        relay, model, {.Color = team == Team::T ? _config.TerroristColor : _config.CtColor});
    if (!glow)
    {
        relay.Remove();
        return;
    }

    pair.Relay = relay.Ref();
    pair.Glow = glow.Ref();
    pair.Team = team;
    pair.Model = std::move(model);

    ShowToViewers(pair.Relay);
    ShowToViewers(pair.Glow);
}

void GlowVision::ShowToViewers(EntityRef entity)
{
    if (_viewerSlot >= 0)
    {
        _visibility.ShowOnlyTo(entity, _viewerSlot);
    }
    else
    {
        _visibility.ShowOnlyToTeam(entity, _viewerTeam);
    }
}

void GlowVision::Refresh()
{
    for (int slot = 0; slot < MaxPlayers; ++slot)
    {
        auto& pair = _pairs[slot];

        Pawn pawn = _entities.Pawn(slot);
        const Team team = pawn.TeamNum();
        // Hidden pawns never reach the viewer, so a clone would follow nothing.
        bool desired = slot != _viewerSlot && pawn && pawn.IsAlive() && IsPlaying(team) &&
                       !_visibility.IsPawnHidden(slot) && (!_config.Filter || _config.Filter(slot));

        if (pair.Active())
        {
            bool stale = !desired || team != pair.Team || !_entities.Get(pair.Relay) || !_entities.Get(pair.Glow) ||
                         pawn.ModelName() != pair.Model;
            if (stale)
            {
                DestroyPair(pair);
            }
        }

        if (!pair.Active() && desired)
        {
            CreatePair(slot, pair);
        }
    }
}

void GlowVision::Destroy()
{
    for (auto& pair : _pairs)
    {
        DestroyPair(pair);
    }
}

}  // namespace VoltMod
