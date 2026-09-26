#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/HookResult.hpp>
#include <VoltMod/Core/Signals/LazyHook.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>

namespace VoltMod
{

/** A player dropping a weapon themselves, as a @ref WeaponDrop::Before handler sees it. */
struct WeaponDropRequest
{
    int Slot = -1;
    /** True when a weapon being picked up pushes this one out; false for the drop key. */
    bool Swapping = false;
    /** Set to keep the weapon; the game drops nothing. */
    bool Blocked = false;
};

/**
 * @brief A player's own weapon drops, from `CCSPlayer_WeaponServices::DropWeapon`.
 *
 * The drop key (G) reaches the server here, not as a `drop` command. The hook runs before the
 * game's own checks, so it sees the key even while `mp_death_drop_gun 0` or `mp_drop_knife_enable 0`
 * keep the weapon in hand. It installs on the first `Before` subscription.
 *
 * @code
 * _drops = runtime.WeaponDrop.Before += [this](VoltMod::WeaponDropRequest& drop) {
 *     if (!drop.Swapping)
 *     {
 *         drop.Blocked = true;
 *         OpenShop(drop.Slot);
 *     }
 * };
 * @endcode
 *
 * Game-thread only.
 */
class WeaponDrop
{
public:
    /** @p entities resolves the dropping player and @p bindings supplies the drop function. Both must
     *  outlive this service; the Runtime declares them above. */
    WeaponDrop(EntitySystem& entities, const Bindings& bindings);
    WeaponDrop(const WeaponDrop&) = delete;
    WeaponDrop& operator=(const WeaponDrop&) = delete;

private:
    LazyHook _hook;

public:
    /** Raised before the game handles a player's drop. */
    Event<WeaponDropRequest&> Before;

    /** Why drops cannot be hooked: the drop signature did not bind. */
    Status Available() const;

private:
    HookResult<bool> OnDrop(EngineWeaponServices& services, bool swapping);

    EntitySystem& _entities;
    const Bindings& _bindings;
};

}  // namespace VoltMod
