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

static Subscription HookTextMessages(IGameEventSystem* system, Event<TextMessage&>& before)
{
    const auto onPost = [&before](IGameEventSystem&, CSplitScreenSlot, bool, int, const uint64*,
                                  INetworkMessageInternal* kind, const CNetMessage* data, unsigned long,
                                  NetChannelBufType_t) { return FilterTextMessage(before, kind, data); };
    // The filter overload posts through this one, so it sees every message.
    return HookInterface(static_cast<PostEvent>(&IGameEventSystem::PostEventAbstract), system, onPost);
}

TextMessages::TextMessages(Interfaces& interfaces)
    : _hook("TextMessages",
            [this]() -> Result<Subscription> {
                if (auto available = Available(); !available)
                {
                    return std::unexpected(available.error());
                }
                return HookTextMessages(_interfaces.GameEventSystem, Before);
            }),
      Before(_hook.ForEvent()),
      _interfaces(interfaces)
{}

Status TextMessages::Available() const
{
    if (!_interfaces.GameEventSystem)
    {
        return std::unexpected(Error::NotReady("IGameEventSystem not available"));
    }
    return {};
}

}  // namespace VoltMod
