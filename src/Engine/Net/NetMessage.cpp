#include "Engine/Net/NetMessage.hpp"

#include <VoltMod/Engine/Interfaces.hpp>
#include <engine2/igameeventsystem.h>
#include <networksystem/inetworkmessages.h>
#include <string>

namespace VoltMod
{

bool PostUserMessage(Interfaces& interfaces, INetworkMessageInternal*& cached, std::string_view partialName,
                     IRecipientFilter& filter, const std::function<bool(CNetMessage*)>& fill)
{
    if (!interfaces.GameEventSystem || !interfaces.NetworkMessages)
    {
        return false;
    }

    if (!cached)
    {
        cached = interfaces.NetworkMessages->FindNetworkMessagePartial(std::string(partialName).c_str());
    }
    if (!cached)
    {
        return false;
    }

    CNetMessage* message = cached->AllocateMessage();
    if (!message)
    {
        return false;
    }

    bool posted = false;
    if (fill(message))
    {
        // The filter overload: the client-bitmask one drops server-originated messages.
        interfaces.GameEventSystem->PostEventAbstract(-1, false, &filter, cached, message, 0);
        posted = true;
    }

    interfaces.NetworkMessages->DeallocateNetMessageAbstract(cached, message);
    return posted;
}

}  // namespace VoltMod
