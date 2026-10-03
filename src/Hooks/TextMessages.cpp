#include <VoltMod/Hooks/TextMessages.hpp>
#include <VoltMod/Unsafe/Hook.hpp>
#include <engine/igameeventsystem.h>
#include <networksystem/inetworkserializer.h>
#include <networksystem/netmessage.h>
#include <usermessages.pb.h>

namespace VoltMod
{

using PostEvent = void (IGameEventSystem::*)(CSplitScreenSlot, bool, int, const uint64*, INetworkMessageInternal*,
                                             const CNetMessage*, unsigned long, NetChannelBufType_t);

static HookResult<void> FilterTextMessage(Event<TextMessage&>& before, INetworkMessageInternal* kind,
                                          const CNetMessage* data)
{
    if (!kind || !data)
    {
        return {};
    }
    if (kind->GetNetMessageInfo()->m_MessageId != UM_TextMsg)
    {
        return {};
    }
    const auto* text = const_cast<CNetMessage*>(data)->ToPB<CUserMessageTextMsg>();
    if (!text || text->param_size() == 0)
    {
        return {};
    }
    TextMessage message{.Text = text->param(0)};
    before.Raise(message);
    return message.Blocked ? HookResult<void>::Block() : HookResult<void>{};
}

TextMessages::TextMessages(Interfaces& interfaces)
    : _hook("TextMessages", [this] { return Install(); }), Before(_hook.ForEvent()), _interfaces(interfaces)
{}

Result<Subscription> TextMessages::Install()
{
    if (auto available = Available(); !available)
    {
        return std::unexpected(available.error());
    }
    const auto onPost = [this](IGameEventSystem&, CSplitScreenSlot, bool, int, const uint64*,
                               INetworkMessageInternal* kind, const CNetMessage* data, unsigned long,
                               NetChannelBufType_t) { return FilterTextMessage(Before, kind, data); };
    // The filter overload posts through this one, so it sees every message.
    return HookInterface(static_cast<PostEvent>(&IGameEventSystem::PostEventAbstract), _interfaces.GameEventSystem,
                         onPost);
}

Status TextMessages::Available() const
{
    if (!_interfaces.GameEventSystem)
    {
        return std::unexpected(Error::NotReady("IGameEventSystem not available"));
    }
    return {};
}

}  // namespace VoltMod
