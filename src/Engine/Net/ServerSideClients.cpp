#include "Engine/Net/ServerSideClients.hpp"

#include <cstdint>

namespace VoltMod
{

const void* FilterClient(const Bindings& bindings, const INetworkMessageProcessingPreFilter& filter)
{
    if (!bindings.ClientMessageFilter)
    {
        return nullptr;
    }
    return reinterpret_cast<const uint8_t*>(&filter) - bindings.ClientMessageFilter.Value();
}

int ClientSlot(const Bindings& bindings, const void* client)
{
    if (!client || !bindings.ClientSlot)
    {
        return -1;
    }
    return bindings.ClientSlot.Read(client);
}

}  // namespace VoltMod
