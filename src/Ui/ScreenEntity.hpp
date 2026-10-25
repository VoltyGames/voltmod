#pragma once

#include "Ui/LayoutPath.hpp"
#include "Ui/WriteCache.hpp"

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Core/Slots/SlotEvents.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Engine/EntityRef.hpp>
#include <VoltMod/Entities/EntitySystem.hpp>
#include <VoltMod/Hooks/Visibility.hpp>
#include <span>
#include <string_view>

namespace VoltMod
{

/**
 * @brief The `custom_hud_layout` entity behind a @ref Screen: spawning, removal, and every write.
 *
 * The entity is networked to its owner alone. Text and classes go to its global state, which a client
 * shows whatever pawn it is watching, and the cursor to the owner's own per-player state, so a screen
 * keeps working while its owner is dead or spectating.
 *
 * Writes never spawn; @ref EnsureSpawned does. Unchanged values are skipped through @ref WriteCache.
 */
class ScreenEntity
{
public:
    /** Every reference must outlive this. @p owner is the player slot the screen belongs to. */
    ScreenEntity(EntitySystem& entities, SlotEvents& slots, Visibility& visibility, LayoutPath layout, int owner);
    ~ScreenEntity();

    ScreenEntity(const ScreenEntity&) = delete;
    ScreenEntity& operator=(const ScreenEntity&) = delete;

    bool Exists() const;

    /** Spawn, or respawn after the player list changed, until the entity covers its owner. */
    bool EnsureSpawned(int slot);

    /** Remove the entity and forget everything written to it. Safe to call again. */
    void Remove();

    void WriteText(int slot, std::string_view variable, std::string_view value);
    void WriteClass(int slot, std::string_view elementId, std::string_view className, bool on);
    /** Turns `<block>--<name>` on, and off only the other @p names the cache holds on: a new entity has none. */
    void WriteModifier(int slot, std::string_view elementId, std::string_view block,
                       std::span<const std::string_view> names, std::string_view name);
    void WriteCursor(int slot, bool shown);

private:
    /** Whether a write naming @p slot is for this screen: its owner or @ref EveryoneSlot. */
    bool Accepts(int slot) const noexcept { return slot == _owner || slot == EveryoneSlot; }

    /** On failure, forget the writes so the next redraw resends them, and log the first one. */
    void CheckWrite(const Status& status, std::string_view what);

    Status Spawn();
    bool SpawnOrWarn();
    bool Covers() const;

    // ScreenEntitySetters.cpp: the entity's state and the game's own setters.
    int PlayerStateCount() const;
    /** The entity, or why it cannot be written to right now. */
    Result<CEntityInstance*> ResolveWritable() const;

    Status SendText(std::string_view variable, std::string_view value);
    Status SendClass(std::string_view elementId, std::string_view className, bool on);
    Status SendCursor(bool shown);

    EntitySystem& _entities;
    Visibility& _visibility;
    LayoutPath _layout;
    int _owner;

    EntityRef _entity;
    /** Per-player state is sized at spawn, so a later slot is only reachable through a new entity. */
    bool _playersChangedSinceSpawn = true;
    WriteCache _written;

    /** Declared last: their handlers touch the members above. */
    Subscription _slotChanges;
    Subscription _fullConnects;
};

}  // namespace VoltMod
