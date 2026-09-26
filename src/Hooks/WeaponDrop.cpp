#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Hooks/WeaponDrop.hpp>
#include <VoltMod/Schema/Api.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <utility>

namespace VoltMod
{

WeaponDrop::WeaponDrop(EntitySystem& entities, const Bindings& bindings)
    : Before({.OnFirst = [this] { return Install(); }, .OnLast = [this] { _hook.Reset(); }}),
      _entities(entities),
      _bindings(bindings)
{}

WeaponDrop::~WeaponDrop()
{
    // A surviving subscription would call into an unloaded module after volt reload.
    if (!Before.Empty())
    {
        Log::Error("WeaponDrop: {} subscription(s) outlived the service; a handler may dangle.", Before.Count());
    }
}

bool WeaponDrop::Install()
{
    auto hook = HookFunction("WeaponDrop", _bindings.DropWeapon,
                             [this](EngineWeaponServices& services, CEntityInstance*, bool swapping) {
                                 return OnDrop(services, swapping);
                             });
    if (!hook)
    {
        Log::Warn("WeaponDrop: {}; drops will not be reported.", hook.error().Detail);
        return false;
    }

    _hook = std::move(*hook);
    return true;
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
