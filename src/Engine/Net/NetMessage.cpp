#include "Engine/Net/NetMessage.hpp"

#include "Engine/Net/MessageTypes.hpp"

#include <VoltMod/Engine/Interfaces.hpp>
#include <engine2/igameeventsystem.h>
#include <networksystem/inetworkmessages.h>

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
        cached = FindMessageType(*interfaces.NetworkMessages, partialName);
    }
    if (!cached)
    {
        return false;
    }

    CNetMessage* message = AllocateMessage(*cached);
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

    FreeMessage(message);
    return posted;
}

}  // namespace VoltMod
