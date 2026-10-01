#include "Hooks/DamageLayout.hpp"

#include <VoltMod/Core/Signals/HookResult.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Events/EventTypes.hpp>
#include <VoltMod/Hooks/Damage.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <cstdint>
#include <igameevents.h>
#include <mathlib/vector.h>
#include <shareddefs.h>
#include <string>
#include <string_view>
#include <utility>

namespace VoltMod
{

static_assert(DamageGeneric == DMG_GENERIC);
static_assert(DamageCrush == DMG_CRUSH);
static_assert(DamageBullet == DMG_BULLET);
static_assert(DamageSlash == DMG_SLASH);
static_assert(DamageBurn == DMG_BURN);
static_assert(DamageFall == DMG_FALL);
static_assert(DamageBlast == DMG_BLAST);
static_assert(DamageClub == DMG_CLUB);
static_assert(DamageShock == DMG_SHOCK);
static_assert(DamageHeadshot == DMG_HEADSHOT);

Damage::Damage(EntitySystem& entities, const Bindings& bindings, Interfaces& interfaces)
    : _hook("Damage",
            [this] {
                return HookFunction("Damage", _bindings.TakeDamage, [this](CEntityInstance& victim, void* info, void*) {
                    return OnTakeDamage(victim, info);
                });
            }),
      Before(_hook.ForEvent()),
      _entities(entities),
      _bindings(bindings),
      _interfaces(interfaces)
{}

HookResult<int64_t> Damage::OnTakeDamage(CEntityInstance& victim, void* rawInfo)
{
    auto* info = static_cast<EngineDamageInfo*>(rawInfo);
    DamageHit hit{.Victim = Entity{_entities, &victim},
                  .Attacker = {static_cast<uint32_t>(info->Attacker.ToInt())},
                  .Inflictor = {static_cast<uint32_t>(info->Inflictor.ToInt())},
                  .Amount = info->Damage,
                  .Type = static_cast<uint32_t>(info->DamageType)};
    Before.Raise(hit);
    if (hit.Blocked)
    {
        return HookResult<int64_t>::Block(1);
    }

    if (hit.Amount != info->Damage)
    {
        // Keep the totalled damage in step with the edited amount, as CS2Fixes does.
        if (info->Damage != 0.0f)
        {
            info->TotalledDamage *= hit.Amount / info->Damage;
        }
        info->Damage = hit.Amount;
    }
    info->DamageType = static_cast<int32_t>(hit.Type);
    return {};
}

Status Damage::Available() const
{
    if (!_bindings.TakeDamage)
    {
        return std::unexpected(Error::Unsupported("the CBaseEntity::TakeDamageOld signature did not bind"));
    }
    if (!_bindings.BuildDamageInfo)
    {
        return std::unexpected(Error::Unsupported("the CTakeDamageInfo constructor signature did not bind"));
    }
    return {};
}

void Damage::Apply(const Entity& victim, const DamageInfo& info)
{
    if (!victim || !Available())
    {
        return;
    }

    CEntityInstance* attacker = _entities.Get(info.Attacker).Raw();
    CEntityInstance* inflictor = _entities.Get(info.Inflictor).Raw();
    // The engine drops a hit with no inflictor.
    if (!inflictor)
    {
        inflictor = attacker;
    }

    // The engine warns about a hit with no position or push, and pushes nothing.
    const Vector position = victim.Origin();
    Vector push = position - Entity{_entities, inflictor}.Origin();
    push.NormalizeInPlace();
    push *= info.Amount;

    EngineDamageInfo damage{};
    _bindings.BuildDamageInfo(&damage, inflictor, attacker, nullptr, &push, &position, info.Amount,
                              static_cast<int>(info.Type), 0, nullptr);

    EngineDamageResult result{};
    result.OriginatingInfo = &damage;
    result.HealthLost = static_cast<int32_t>(info.Amount);
    result.DamageDealt = info.Amount;
    result.PreModifiedDamage = info.Amount;
    result.TotalledHealthLost = static_cast<int32_t>(info.Amount);
    result.TotalledDamageDealt = info.Amount;

    if (!info.Weapon.empty())
    {
        HookDeathEvents();
    }
    // A death handler may deal its own hit inside this one.
    const std::string_view outer = std::exchange(_weapon, info.Weapon);
    _bindings.TakeDamage(victim.Raw(), &damage, &result);
    _weapon = outer;
}

void Damage::HookDeathEvents()
{
    if (_deathEvents || !_interfaces.GameEventManager)
    {
        return;
    }
    _deathEvents = HookInterface(&IGameEventManager2::FireEvent, _interfaces.GameEventManager,
                                 [this](IGameEventManager2&, IGameEvent* event, bool) {
                                     const bool death = event && event->GetName() == PlayerDeath::EventName;
                                     if (death && !_weapon.empty())
                                     {
                                         event->SetString("weapon", std::string(_weapon).c_str());
                                     }
                                 });
}

}  // namespace VoltMod
