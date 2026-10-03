#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Hooks/WeaponDrop.hpp>
#include <VoltMod/Schema/Api.hpp>
#include <VoltMod/Unsafe/Hook.hpp>

namespace VoltMod
{

WeaponDrop::WeaponDrop(EntitySystem& entities, const Bindings& bindings)
    : _hook("WeaponDrop", [this] { return Install(); }),
      Before(_hook.ForEvent()),
      _entities(entities),
      _bindings(bindings)
{}

Result<Subscription> WeaponDrop::Install()
{
    const auto onDrop = [this](EngineWeaponServices& services, CEntityInstance*, bool swapping) {
        return OnDrop(services, swapping);
    };
    return HookFunction("WeaponDrop", _bindings.DropWeapon, onDrop);
}

HookResult<bool> WeaponDrop::OnDrop(EngineWeaponServices& services, bool swapping)
{
    CEntityInstance* pawn = Schema::CPlayer_WeaponServices{&services}.OwnerEntity();
    WeaponDropRequest drop{.Slot = pawn ? Pawn{_entities, pawn}.Slot() : -1, .Swapping = swapping};
    if (!IsValidSlot(drop.Slot))
    {
        return {};
    }

    Before.Raise(drop);
    return drop.Blocked ? HookResult<bool>::Block(false) : HookResult<bool>{};
}

Status WeaponDrop::Available() const
{
    if (!_bindings.DropWeapon)
    {
        return std::unexpected(Error::Unsupported("the CCSPlayer_WeaponServices::DropWeapon signature did not bind"));
    }
    return {};
}

}  // namespace VoltMod
