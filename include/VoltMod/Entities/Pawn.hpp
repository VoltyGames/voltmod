#pragma once

#include <VoltMod/Engine/Server/Clock.hpp>
#include <VoltMod/Entities/Entity.hpp>
#include <VoltMod/Entities/ObserverMode.hpp>
#include <VoltMod/Schema/Generated/CCSPlayerPawn.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** The tick to hold fire until when holding it every frame: past what a client predicts, so it shows
 *  no shot or recoil, and moved in steps, so the weapon's networked ticks change once a step. */
int HoldFireTick(const Clock& clock);

/** A standing player's collision box, in units from their feet. */
struct PlayerHull
{
    static constexpr float HalfWidth = 16.0f;
    static constexpr float Height = 72.0f;
    /** The tallest ledge a player walks up without jumping. */
    static constexpr float StepHeight = 18.0f;
    static inline const Vector Mins{-HalfWidth, -HalfWidth, 0.0f};
    static inline const Vector Maxs{HalfWidth, HalfWidth, Height};
};

/**
 * @brief A player's body: health, armor, movement, aim, weapons. The engine replaces it on every
 * spawn; the @ref Controller is the identity that stays.
 */
class Pawn : public Entity
{
public:
    using Entity::Entity;

    /** @name CCSPlayerPawn, CBasePlayerPawn and CBaseModelEntity fields
     *  `VelocityModifier` decays back toward 1 on its own, and `ViewOffset` is the eye height. */
    /** @{ */
#include <VoltMod/Schema/Generated/Wrappers/Pawn.inc>
    /** @} */

    bool IsAlive() const { return _e != nullptr && LifeState() == LIFE_ALIVE; }

    bool IsOnGround() const { return (Flags() & FL_ONGROUND) != 0; }

    /** Where shots start: the origin plus the view offset. */
    Vector EyePosition() const;

    /** The unit vector the player aims along. */
    Vector EyeForward() const;

    /** Does nothing when the CommitSuicide index did not bind. */
    void Slay() const;

    /** The FL_GODMODE flag, which is what makes a CS2 pawn take no damage. */
    bool Godmode() const { return (Flags() & FL_GODMODE) != 0; }
    void SetGodmode(bool on) const;

    /** Set the pawn's velocity and lift it off the ground this tick, so the engine keeps the velocity.
     *  SetAbsVelocity alone leaves a grounded pawn grounded. */
    void SetVelocity(Vector velocity) const;

    /** Add @p amount health, up to MaxHealth. False when nothing changed, such as a dead pawn. */
    bool Heal(int amount) const;

    /** Give an item by class name, such as "weapon_ak47". A weapon the pawn's team cannot buy is
     *  given through a same-frame team swap. The given item; falsy when the engine refused it. */
    Entity GiveItem(std::string_view item) const;

    /** Remove every weapon, and with @p removeSuit the armor and defuse kit too. */
    bool StripWeapons(bool removeSuit = true) const;

    /** The carried weapons, knife and grenades included. */
    std::vector<Entity> Weapons() const;

    /** Draw @p weapon. Does nothing for a weapon the pawn does not carry, or when the SelectItem index
     *  did not bind. */
    void SelectWeapon(const Entity& weapon) const;

    /** Remove @p weapon through the engine's input queue, so it is safe inside a death event. A weapon in a
     *  live pawn's hand is first swapped for the one held before it. */
    void RemoveWeapon(const Entity& weapon) const;

    /** The weapon in hand; falsy when there is none. */
    Entity ActiveWeapon() const;

    /** Keep the weapon in hand from either attack before engine tick @p tick. A weapon switched to
     *  later is not covered, so call it every frame to hold fire for a while. */
    void HoldFire(int tick) const;

    /** @ref HoldFire until @ref HoldFireTick; call it every frame to hold fire. */
    void HoldFire(const Clock& clock) const;

    /** Keep the pawn crouched whatever its player holds; call it every frame to hold it. Each step of
     *  @p amount past 1 lowers the eye 18 units more. */
    void HoldCrouch(float amount = 1.0f) const;
    /** Let the pawn stand up again once its player lets go of crouch. */
    void ReleaseCrouch() const;

    VoltMod::ObserverMode ObserverMode() const;
    Status SetObserverMode(VoltMod::ObserverMode mode) const;

    /** Current model path; empty when unavailable. */
    std::string ModelName() const;

    /** The owning player's controller. */
    VoltMod::Controller Controller() const;

    /** The owning player's slot, or -1. Constant time. */
    int Slot() const;
};

}  // namespace VoltMod
