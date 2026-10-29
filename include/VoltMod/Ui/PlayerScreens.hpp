#pragma once

#include <VoltMod/Core/Slots/PerSlot.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Ui/Screen.hpp>
#include <VoltMod/Ui/ScreenManager.hpp>
#include <optional>
#include <string>

namespace VoltMod
{

/**
 * @brief One layout's player screens, each created the first time something draws for that slot.
 *
 * A player screen costs an entity, so this makes one per player who is actually shown the layout
 * rather than one per connected player. A screen removes its entity when its slot changes hands
 * and respawns for the next player on @ref Screen::EnsureSpawned.
 */
class PlayerScreens
{
public:
    /** @p screens must outlive this. */
    PlayerScreens(ScreenManager& screens, std::string layout);

    /** @p slot's screen, created on first use. One @ref ScreenManager::Create refuses is logged
     *  once and stays empty, so its writes fail quietly. */
    Screen& For(int slot);

    /** @p slot's screen if one was created, or null. Never creates. */
    Screen* Find(int slot);

    /** @p slot's screen created and spawned, ready to draw; null for a bot, an empty slot, or a screen
     *  that cannot spawn. */
    Screen* FindSpawned(int slot);

    /** @ref FindSpawned, then shows the layout's root element and gives the player a cursor, so it takes
     *  their clicks until @ref Close. Null when it cannot spawn. */
    Screen* Open(int slot);

    /** Takes the cursor back and removes the entity: clients draw only their first custom_hud_layout, so a
     *  hidden one would block the next. */
    void Close(int slot);

    /** Whether @p slot's screen is open: after @ref Open, until @ref Close, the slot changing hands or a
     *  map change removing the entity. */
    bool IsOpen(int slot) const;

private:
    ScreenManager& _screens;
    std::string _layout;
    /** The layout's root element, which @ref Open shows. */
    std::string _root;
    PerSlot<std::optional<Screen>> _created;
    /** Handed out for a slot that is not a player slot. */
    Screen _empty;
};

}  // namespace VoltMod
