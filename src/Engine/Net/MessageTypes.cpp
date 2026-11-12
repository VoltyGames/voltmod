#include "Engine/Net/MessageTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <networksystem/inetworkmessages.h>
#include <string>

namespace VoltMod
{

/** The engine's record behind an INetworkMessageInternal pointer; only the fields read here. */
struct MessageTypeRecord
{
    const char* Name;  // a CUtlString, which holds only its buffer
    CNetMessage* (*Allocate)();
    const char* Group;
    int32_t Id;
};
static_assert(offsetof(MessageTypeRecord, Allocate) == 0x08);
static_assert(offsetof(MessageTypeRecord, Id) == 0x18);

/** CNetworkMessages::FindNetworkMessagePartial on both platforms; s2sdk still places it at 14. */
static constexpr size_t FindPartialSlot = 10;

static const MessageTypeRecord& Record(const INetworkMessageInternal& type)
{
    return *reinterpret_cast<const MessageTypeRecord*>(&type);
}

INetworkMessageInternal* FindMessageType(INetworkMessages& messages, std::string_view partialName)
{
    // With only pointer arguments, both ABIs call a virtual like a free function taking `this` first.
    using FindPartial = INetworkMessageInternal* (*)(INetworkMessages*, const char*);
    void* const* slots = *reinterpret_cast<void* const* const*>(&messages);
    const auto find = reinterpret_cast<FindPartial>(slots[FindPartialSlot]);
    return find(&messages, std::string(partialName).c_str());
}

int ReadMessageId(const INetworkMessageInternal& type)
{
    return Record(type).Id;
}

CNetMessage* AllocateMessage(const INetworkMessageInternal& type)
{
    const auto allocate = Record(type).Allocate;
    return allocate ? allocate() : nullptr;
}

void FreeMessage(CNetMessage* message)
{
    // The engine's deleting destructor frees it with the allocator that made it.
    delete message;
}

}  // namespace VoltMod
