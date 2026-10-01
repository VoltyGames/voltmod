#pragma once

#include <igameevents.h>

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/CallbackRegistry.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/Interfaces.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace VoltMod
{

/** @brief Creates, fires and listens to game events. */
class GameEvents : public IGameEventListener2
{
public:
    /** Reads IGameEventManager2 into @p interfaces. Both must outlive this service. */
    GameEvents(Interfaces& interfaces, const Bindings& bindings);
    ~GameEvents() override;
    GameEvents(const GameEvents&) = delete;
    GameEvents& operator=(const GameEvents&) = delete;

    /** An error when the event manager did not resolve. */
    Status Available() const;

    IGameEvent* CreateEvent(std::string_view name);
    bool FireEvent(IGameEvent* event, bool broadcast = true);
    void FreeEvent(IGameEvent* event);

    /** Subscribe to @p TEvent, a struct from `<VoltMod/Events/EventTypes.hpp>`, while the returned
     *  Subscription lives. An event whose `Slot` is invalid never reaches @p handler. */
    template <class TEvent>
    [[nodiscard]] Subscription On(std::function<void(const TEvent&)> handler)
    {
        return Add(TEvent::EventName, [h = std::move(handler)](IGameEvent* e) {
            if (!e)
            {
                return;
            }
            const TEvent event = TEvent::From(*e);
            if constexpr (requires { event.Slot; })
            {
                if (!IsValidSlot(event.Slot))
                {
                    return;
                }
            }
            h(event);
        });
    }

    /** Remove every handler and detach from the engine. */
    void RemoveAllListeners();

    /** Re-attach every event that still has a handler: map startup resets the engine's listeners. */
    void OnServerStartup();

    /** @p slot's client listener: an event fired at it reaches that client alone. Null without a
     *  client or when the GetLegacyGameEventListener signature did not bind. */
    IGameEventListener2* GetClientLegacyListener(int slot) const;

    /** Whether @p slot's client listens to @p eventName; a vanilla client takes only what its HUD
     *  needs, so an extra one can mean injected code. */
    bool ClientListensTo(int slot, std::string_view eventName) const;

    void FireGameEvent(IGameEvent* event) override;

private:
    using EventCallback = std::function<void(IGameEvent*)>;

    [[nodiscard]] Subscription Add(std::string_view eventName, EventCallback callback);

    struct EventHandlers
    {
        CallbackRegistry<EventCallback> Handlers;
        bool Attached = false;
    };

    using GetLegacyGameEventListenerFn = IGameEventListener2* (*)(CPlayerSlot slot);

    Interfaces& _interfaces;
    const Bindings& _bindings;
    /** Never erased: subscriptions point at their entry's registry. */
    std::map<std::string, EventHandlers, std::less<>> _events;
    GetLegacyGameEventListenerFn _getLegacyListener = nullptr;
};

}  // namespace VoltMod
