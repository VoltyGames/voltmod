#include <VoltMod/Entities/FireHold.hpp>
#include <VoltMod/Schema/Generated/CBasePlayerWeapon.hpp>

namespace VoltMod
{

void FireHold::Hold(const Pawn& pawn, int tick)
{
    const Entity active = pawn.ActiveWeapon();
    const Schema::CBasePlayerWeapon weapon{active.Raw()};
    if (weapon && active.Ref() != _weapon)
    {
        _weapon = active.Ref();
        _primary = weapon.NextPrimaryAttackTick();
        _secondary = weapon.NextSecondaryAttackTick();
    }
    _until = tick;
    pawn.HoldFire(_until);
}

void FireHold::Release(const Pawn& pawn)
{
    if (_until == 0)
    {
        return;
    }
    const Entity active = pawn.ActiveWeapon();
    const Schema::CBasePlayerWeapon weapon{active.Raw()};
    // Ticks a reload or a switch has set since are the game's own, so only the hold's are put back.
    if (weapon && active.Ref() == _weapon)
    {
        if (weapon.NextPrimaryAttackTick() == _until)
        {
            weapon.SetNextPrimaryAttackTick(_primary);
        }
        if (weapon.NextSecondaryAttackTick() == _until)
        {
            weapon.SetNextSecondaryAttackTick(_secondary);
        }
    }
    _weapon = {};
    _until = 0;
}

}  // namespace VoltMod
