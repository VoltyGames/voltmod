#include "Schema/Notify.hpp"

#include <VoltMod/Engine/Memory/MemoryAccess.hpp>
#include <entity2/entityinstance.h>
#include <networksystem/networkvar.h>

namespace VoltMod::Schema
{

void NotifyEntity(CEntityInstance* entity, int32_t offset)
{
    if (!entity)
    {
        return;
    }

    entity->NetworkStateChanged(NetworkStateChangedData(static_cast<uint32>(offset)));
}

void NotifyComponentOwner(void* component, int32_t ownerLinkOffset, int32_t offset)
{
    if (!component || ownerLinkOffset < 0)
    {
        return;
    }

    // The chainer adds its path index and, like the engine, skips an owner still being constructed.
    NetworkStateChangedData change(static_cast<uint32>(offset));
    MemberPtr<CNetworkVarChainer>(component, ownerLinkOffset)->NetworkStateChanged(change);
}

CEntityInstance* ComponentOwner(const void* component, int32_t ownerLinkOffset)
{
    if (!component || ownerLinkOffset < 0)
    {
        return nullptr;
    }
    // MemberPtr takes a mutable base; the read here never writes through it.
    return *MemberPtr<CNetworkVarChainer>(const_cast<void*>(component), ownerLinkOffset);
}

}  // namespace VoltMod::Schema
