#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/Detours.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Hooks/Teleport.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <mathlib/vector.h>

namespace VoltMod
{

static Result<Subscription> HookTeleport(const Bindings& bindings, EntitySystem& entities, Event<int>& before)
{
    const auto onTeleport = [&entities, &before](CEntityInstance& pawn, const Vector*, const QAngle*, const Vector*) {
        // Through the controller, so a recycled pawn address cannot mislead.
        if (const int slot = Pawn{entities, &pawn}.Slot(); IsValidSlot(slot))
        {
            before.Raise(slot);
        }
    };
    return HookVirtual("Teleport", bindings.Teleport, onTeleport);
}

Teleport::Teleport(EntitySystem& entities, const Bindings& bindings)
    : _hook("Teleport", [this] { return HookTeleport(_bindings, _entities, Before); }),
      Before(_hook.ForEvent()),
      _entities(entities),
      _bindings(bindings)
{}

Status Teleport::Available() const
{
    if (!_bindings.Teleport)
    {
        return std::unexpected(Error::Unsupported("the CBaseEntity::Teleport vtable slot did not bind"));
    }
    return {};
}

}  // namespace VoltMod
