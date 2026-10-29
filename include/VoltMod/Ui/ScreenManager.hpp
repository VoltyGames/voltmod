#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/SlotEvents.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/Net/NetChannel.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Hooks/Visibility.hpp>
#include <VoltMod/Ui/ButtonPress.hpp>
#include <VoltMod/Ui/Screen.hpp>
#include <functional>
#include <string_view>

namespace VoltMod
{

/**
 * @brief Create @ref Screen objects and report their button presses.
 *
 * Every screen belongs to one player: a screen everyone received would send each player's state to
 * every client. @p layout must be a bare name (`"welcome"`) or a resource name under
 * `panorama/layout/custom_game/` with its source `.xml` extension. Other forms are rejected before
 * reaching the client.
 */
class ScreenManager
{
public:
    /** Subscribes a ScreenManager to the host's button presses; dropping the result unsubscribes. */
    using Connector = std::function<Subscription(ScreenManager&)>;

    /** The services must outlive this one; @p connect runs when the first press handler arrives. */
    ScreenManager(EntitySystem& entities, const Bindings& bindings, SlotEvents& slots, Visibility& visibility,
                  const NetChannels& clients, Connector connect);

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    /** Return the reason screens are unavailable, or success when all required bindings exist. */
    Status Available() const;

    /** A screen only @p slot receives, removed when the slot changes hands. Spawned on its first
     *  @ref Screen::EnsureSpawned. */
    Result<Screen> Create(std::string_view layout, int slot);

    /** Whether @p slot holds a human client a screen can reach: false for a bot, an empty slot or one
     *  disconnecting. */
    bool HasClient(int slot) const;

    /** Every button press from every layout; filter on @ref ButtonPress::ButtonId. A subscription is
     *  refused when the client filter table did not bind. */
    Event<const ButtonPress&> Pressed;

    /** @internal A press the host read, on the frame after it arrived. */
    void OnPress(int slot, std::string_view buttonId);

private:
    bool WatchPresses();

    EntitySystem& _entities;
    const Bindings& _bindings;
    SlotEvents& _slots;
    Visibility& _visibility;
    const NetChannels& _clients;
    Connector _connect;
    /** Declared after @ref Pressed: unsubscribes before the event goes. */
    Subscription _host;
};

}  // namespace VoltMod
