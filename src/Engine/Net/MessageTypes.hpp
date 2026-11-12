#pragma once

#include <VoltMod/Engine/EngineTypes.hpp>
#include <string_view>

namespace VoltMod
{

// Game build 2000943 made a message type a plain record and dropped four INetworkMessages virtuals.
// Until s2sdk follows, its declarations of both crash: message types go through here.

/** The registered message type whose name contains @p partialName, or null. */
INetworkMessageInternal* FindMessageType(INetworkMessages& messages, std::string_view partialName);

/** The id the engine registered @p type under. */
int ReadMessageId(const INetworkMessageInternal& type);

/** A new message of @p type, or null. Release it with FreeMessage. */
CNetMessage* AllocateMessage(const INetworkMessageInternal& type);

/** Release a message from AllocateMessage. */
void FreeMessage(CNetMessage* message);

}  // namespace VoltMod
