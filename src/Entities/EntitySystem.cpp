#include "Engine/Memory/VtableLookup.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <entity2/entityidentity.h>
#include <entity2/entityinstance.h>
#include <entity2/entitysystem.h>
#include <entityhandle.h>
#include <gameentitysystem.h>
#include <interfaces/interfaces.h>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

static constexpr std::string_view ControllerClass = "cs_player_controller";

static_assert(MaxPlayers == ABSOLUTE_PLAYER_LIMIT);

EntitySystem::EntitySystem(VoltMod::Interfaces& interfaces, const VoltMod::Bindings& bindings)
    : _interfaces(interfaces), _bindings(bindings)
{
    if (_interfaces.GameResourceService)
    {
        Raw();
    }
}

CGameEntitySystem* EntitySystem::ReadEntitySystemPointer()
{
    // Retry until the pointer resolves; keep this path a plain read.
    if (!_interfaces.GameResourceService || _isRealSystem == false)
    {
        return nullptr;
    }

    CGameEntitySystem* system = ::GameEntitySystem();
    // A game update can move the field before the SDK follows; check the vtable before using it.
    if (system && !_isRealSystem)
    {
        const void* table = FindVirtualTable("server", "CGameEntitySystem");
        _isRealSystem = !table || HasVtable(system, table);
        if (!*_isRealSystem)
        {
            Log::Error(
                "The resource service's entity system field is not a CGameEntitySystem; entity lookups are off.");
            return nullptr;
        }
    }
    return system;
}

void EntitySystem::OnServerStartup()
{
    // Each map creates a new system, so discard the old pointer before it is freed.
    _interfaces.EntitySystem = nullptr;

    if (Raw())
    {
        Log::Info("Entity system initialized.");
    }
    else
    {
        Log::Error("Entity system pointer could not be read from IGameResourceService.");
    }
}

CGameEntitySystem* EntitySystem::Raw()
{
    // The cache stays empty until the first server starts and is cleared for each new map.
    if (!_interfaces.EntitySystem)
    {
        _interfaces.EntitySystem = ReadEntitySystemPointer();
    }

    return _interfaces.EntitySystem;
}

Entity EntitySystem::Get(EntityRef ref)
{
    auto* system = Raw();
    if (!ref || !system)
    {
        return {};
    }

    // The identity's serial must match, since a destroyed entity leaves a dangling instance pointer.
    return {*this, system->GetEntityInstance(CEntityHandle(ref.Handle))};
}

CEntityInstance* EntitySystem::RawController(int slot)
{
    auto* system = Raw();
    if (!system || slot < 0 || slot >= MaxPlayers)
    {
        return nullptr;
    }

    // Controllers occupy indices 1..MaxPlayers; index 0 is worldspawn.
    CEntityIdentity* identity = system->GetEntityIdentity(CEntityIndex(slot + 1));
    if (!identity)
    {
        return nullptr;
    }

    // An index past the server's player limit holds an ordinary entity.
    if (ControllerClass != identity->GetClassname())
    {
        return nullptr;
    }
    return identity->m_pInstance;
}

VoltMod::Controller EntitySystem::Controller(int slot)
{
    return {*this, RawController(slot), slot};
}

VoltMod::Pawn EntitySystem::Pawn(int slot)
{
    return Controller(slot).Pawn();
}

VoltMod::Pawn EntitySystem::Pawn(EntityRef ref)
{
    return Get(ref).AsPawn();
}

Entity EntitySystem::Find(std::string_view className)
{
    if (!Raw() || className.empty())
    {
        return {};
    }

    // Walk the active list: the SDK's CEntityClass layout is stale, so its per-class chain reads garbage.
    const std::string name(className);
    EntityInstanceByClassIter_t iter(nullptr, name.c_str());
    return {*this, iter.Next()};
}

std::vector<Entity> EntitySystem::FindAll(std::string_view className)
{
    std::vector<Entity> found;
    if (!Raw() || className.empty())
    {
        return found;
    }

    const std::string name(className);
    EntityInstanceByClassIter_t iter(nullptr, name.c_str());
    for (CEntityInstance* entity = iter.Next(); entity; entity = iter.Next())
    {
        found.emplace_back(*this, entity);
    }
    return found;
}

}  // namespace VoltMod
