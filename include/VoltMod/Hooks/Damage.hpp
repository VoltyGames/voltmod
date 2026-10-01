#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/HookResult.hpp>
#include <VoltMod/Core/Signals/LazyHook.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Engine/EntityRef.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <VoltMod/Entities/Entity.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Events/GameEvents.hpp>
#include <cstdint>
#include <string_view>

namespace VoltMod
{

/** @defgroup DamageTypes Damage type bits (DamageTypes_t); combine with `|`. */
/** @{ */
constexpr uint32_t DamageGeneric = 0;
constexpr uint32_t DamageCrush = 1u << 0;
constexpr uint32_t DamageBullet = 1u << 1;
constexpr uint32_t DamageSlash = 1u << 2;
constexpr uint32_t DamageBurn = 1u << 3;
constexpr uint32_t DamageFall = 1u << 5;
constexpr uint32_t DamageBlast = 1u << 6;
constexpr uint32_t DamageClub = 1u << 7;
constexpr uint32_t DamageShock = 1u << 8;
constexpr uint32_t DamageHeadshot = 1u << 19;
/** @} */

/** Who deals the damage, how much, and of which kind. */
struct DamageInfo
{
    EntityRef Attacker;   ///< credited in the kill feed and `player_death`
    EntityRef Inflictor;  ///< what did it, such as a grenade or a turret; empty means the attacker
    float Amount = 0.0f;
    uint32_t Type = DamageGeneric;  ///< @ref DamageTypes bits
    /** The weapon a kill's `player_death` names, and so the kill feed's icon
     *  (`panorama/images/icons/equipment/<Weapon>.svg`); empty keeps the engine's. */
    std::string_view Weapon;
    /** What the victim's own `player_death` names, for their death panel; empty sends everyone @ref Weapon. */
    std::string_view VictimWeapon;
};

/** One hit on its way to the engine, as a @ref Damage::Before handler sees it. */
struct DamageHit
{
    Entity Victim;
    /** Read-only: the engine keeps its own copy of who is credited. */
    const EntityRef Attacker;
    const EntityRef Inflictor;
    /** Edits to Amount and Type reach the engine. */
    float Amount = 0.0f;
    uint32_t Type = DamageGeneric;  ///< @ref DamageTypes bits
    /** Set to cancel the hit; nothing is dealt and no damage event fires. */
    bool Blocked = false;
};

/**
 * @brief Every entity's damage, from `CBaseEntity::TakeDamageOld`, and a way to deal it.
 *
 * The hook sits where the function starts, so it sees players, props and anything else that takes
 * damage, including hits from @ref Apply. It installs on the first `Before` subscription.
 *
 * @code
 * _damage = runtime.Damage.Before += [this](VoltMod::DamageHit& hit) {
 *     if (IsStructure(hit.Victim.Ref()))
 *         hit.Blocked = HitStructure(hit.Victim.Ref(), hit.Attacker, hit.Amount);
 * };
 * runtime.Damage.Apply(bot, {.Attacker = owner.Ref(), .Inflictor = turret, .Amount = 25,
 *                                  .Type = VoltMod::DamageBullet});
 * @endcode
 *
 * Game-thread only.
 */
class Damage
{
public:
    /** @p entities resolves the refs, @p bindings supplies the two damage functions, @p interfaces the
     *  event manager and @p events each client's listener. All must outlive this service; the Runtime
     *  declares them above. */
    Damage(EntitySystem& entities, const Bindings& bindings, Interfaces& interfaces, GameEvents& events);
    Damage(const Damage&) = delete;
    Damage& operator=(const Damage&) = delete;

private:
    LazyHook _hook;

public:
    /** Raised before the engine applies a hit. */
    Event<DamageHit&> Before;

    /** Why damage cannot be hooked or dealt: a damage signature did not bind. */
    Status Available() const;

    /**
     * Deal @p info to @p victim through the engine, so death, the kill feed and `player_death`
     * credit @p info's attacker as if its own weapon had hit. Does nothing for a falsy victim or
     * while @ref Available fails.
     */
    void Apply(const Entity& victim, const DamageInfo& info);

private:
    HookResult<int64_t> OnTakeDamage(CEntityInstance& victim, void* info);
    /** Renames the weapon in each `player_death` fired while @ref Apply deals a named weapon's hit. */
    void HookDeathEvents();
    /** Fires @p death at each client, with @p info's @ref DamageInfo::VictimWeapon for the victim. */
    void SendDeath(IGameEvent& death, const DamageInfo& info);

    EntitySystem& _entities;
    const Bindings& _bindings;
    Interfaces& _interfaces;
    GameEvents& _events;
    /** The hit @ref Apply is dealing, or null. */
    const DamageInfo* _hit = nullptr;
    Subscription _deathEvents;
};

}  // namespace VoltMod
