#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Schema/Generated/CBeam.hpp>
#include <VoltMod/Schema/Generated/CParticleSystem.hpp>
#include <string>
#include <utility>

namespace VoltMod
{

// The engine's SolidType_t values the `solid` key takes.
static constexpr int SolidNone = 0;
static constexpr int SolidVPhysics = 6;
static constexpr int GlowTeamAny = -1;
static constexpr int GlowStateAlwaysOn = 3;
// Draws the glow outline alone.
static constexpr int OutlineRenderAmount = 1;
// An info_target with this flag reaches the clients, so a particle can read its position.
static constexpr int TargetSentToClients = 1;

Status EntitySystem::Available() const
{
    if (!_interfaces.GameResourceService)
    {
        return std::unexpected(Error::NotReady("IGameResourceService not available"));
    }
    if (!_bindings.CreateEntityByName || !_bindings.DispatchSpawn)
    {
        return std::unexpected(Error::Unsupported("CreateEntityByName or DispatchSpawn did not bind"));
    }
    return {};
}

Entity EntitySystem::Create(std::string_view className)
{
    // Checked here so every create-then-spawn path gets it.
    if (!Available() || className.empty())
    {
        return {};
    }
    return {*this, _bindings.CreateEntityByName(std::string(className).c_str(), -1)};
}

Entity EntitySystem::Spawn(std::string_view className, KeyValues& values)
{
    Entity entity = Create(className);
    entity.Spawn(values);
    return entity;
}

Entity EntitySystem::SpawnProp(const PropSpec& prop)
{
    KeyValues values;
    values.Set("model", prop.Model)
        .Set("origin", prop.Origin)
        .Set("angles", prop.Angles)
        .Set("solid", prop.Solid ? SolidVPhysics : SolidNone);
    if (!prop.Skin.empty())
    {
        values.Set("skin", prop.Skin);
    }
    if (!prop.CastsShadow)
    {
        values.Set("disableshadows", 1);
    }
    if (prop.Glow)
    {
        const PropGlow& glow = *prop.Glow;
        const int team = glow.Team == Team::None ? GlowTeamAny : std::to_underlying(glow.Team);
        values.Set("glowcolor", glow.Color)
            .Set("glowrange", glow.Range)
            .Set("glowteam", team)
            .Set("glowstate", GlowStateAlwaysOn);
        if (glow.OutlineOnly)
        {
            values.Set("renderamt", OutlineRenderAmount);
        }
    }

    Entity entity = Create("prop_dynamic");
    // The physics shapes take their owner as they are created.
    if (prop.Owner)
    {
        entity.SetOwnerEntity(prop.Owner);
    }
    entity.Spawn(values);
    // "solid" 0 alone leaves the model's hull blocking traces.
    if (entity && !prop.Solid)
    {
        entity.AcceptInput("DisableCollision");
    }
    if (entity && prop.Scale != 1.0f)
    {
        entity.SetScale(prop.Scale);
    }
    return entity;
}

Entity EntitySystem::SpawnParticle(std::string_view effect, const Vector& origin, const QAngle& angles)
{
    if (effect.empty())
    {
        return {};
    }
    KeyValues values;
    values.Set("effect_name", effect).Set("start_active", 1).Set("origin", origin).Set("angles", angles);
    return Spawn("info_particle_system", values);
}

Entity EntitySystem::SpawnLine(std::string_view effect, const Vector& from, EntityRef end)
{
    if (effect.empty())
    {
        return {};
    }
    Entity line = Create("info_particle_system");
    const Schema::CParticleSystem particle{line.Raw()};
    if (!particle)
    {
        return {};
    }
    // Before it spawns: an effect that starts active reads its control points then.
    particle.SetControlPointRef(1, end);

    KeyValues values;
    values.Set("effect_name", effect).Set("start_active", 1).Set("origin", from);
    line.Spawn(values);
    return line;
}

Entity EntitySystem::SpawnLine(std::string_view effect, const Vector& from, const Vector& to)
{
    if (effect.empty())
    {
        return {};
    }
    KeyValues values;
    values.Set("origin", to).Set("spawnflags", TargetSentToClients);
    const Entity end = Spawn("info_target", values);
    if (!end)
    {
        return {};
    }
    const Entity line = SpawnLine(effect, from, end.Ref());
    if (!line)
    {
        end.Remove();
        return {};
    }
    end.SetParent(line);
    return line;
}

Entity EntitySystem::SpawnBeam(const Vector& from, const Vector& to, float width, Color color)
{
    // Not env_beam: a spawned env_beam brings the server down within seconds.
    Entity line = Create("beam");
    const Schema::CBeam beam{line.Raw()};
    if (!beam)
    {
        return {};
    }
    beam.SetWidth(width);
    beam.SetEndWidth(width);
    beam.SetEndPos(to);
    line.SetRender(Schema::RenderMode_t::kRenderNormal, color);

    KeyValues values;
    values.Set("origin", from);
    line.Spawn(values);
    return line;
}

}  // namespace VoltMod
