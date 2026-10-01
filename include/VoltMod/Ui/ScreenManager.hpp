#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Event.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/SlotEvents.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
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
 * @p layout must be a bare name (`"welcome"`) or a resource name under
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
                  Connector connect);

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    /** Return the reason screens are unavailable, or success when all required bindings exist. */
    Status Available() const;

    /** A screen every player receives. Spawned on its first @ref Screen::EnsureSpawned. */
    Result<Screen> Shared(std::string_view layout);

    /**
     * A screen only @p slot receives, removed when the slot changes hands. This requires
     * @ref Visibility::Available because otherwise the entity would reach everyone.
     */
    Result<Screen> ForPlayer(std::string_view layout, int slot);

    /** Every button press from every layout; filter on @ref ButtonPress::ButtonId. A subscription is
     *  refused when the FilterMessage bindings are missing. */
    Event<const ButtonPress&> Pressed;

    /** @internal A press the host read, on the frame after it arrived. */
    void OnPress(int slot, std::string_view buttonId);

private:
    Result<Screen> Create(std::string_view layout, int owner);
    bool ListenForPresses();

    EntitySystem& _entities;
    const Bindings& _bindings;
    SlotEvents& _slots;
    Visibility& _visibility;
    Connector _connect;
    /** Declared after @ref Pressed: unsubscribes before the event goes. */
    Subscription _host;
};

}  // namespace VoltMod
