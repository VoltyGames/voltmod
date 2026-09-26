#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/Detours.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Hooks/Teleport.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <mathlib/vector.h>

namespace VoltMod
{

Teleport::Teleport(EntitySystem& entities, const Bindings& bindings)
    : _hook("Teleport",
            [this] {
                return HookVirtual("Teleport", _bindings.Teleport,
                                   [this](CEntityInstance& pawn, const Vector*, const QAngle*, const Vector*) {
                                       // Resolve through the controller so a recycled pawn address cannot misidentify it.
                                       Teleported.Raise(Pawn{_entities, &pawn}.Slot());
                                   });
            }),
      Teleported(_hook.ForEvent()),
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
