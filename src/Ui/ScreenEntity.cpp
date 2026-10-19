#include "Ui/ScreenEntity.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Entities/Entity.hpp>
#include <VoltMod/Entities/KeyValues.hpp>
#include <format>
#include <utility>

namespace VoltMod
{

ScreenEntity::ScreenEntity(EntitySystem& entities, SlotEvents& slots, Visibility& visibility, LayoutPath layout,
                           int owner)
    : _entities(entities), _visibility(visibility), _layout(std::move(layout)), _owner(owner)
{
    _slotChanges = slots.Changed += [this](int slot) {
        _playersChangedSinceSpawn = true;
        if (slot == _owner)  // a player screen leaves with its owner
        {
            Remove();
        }
        else
        {
            _written.RemoveSlot(slot);
        }
    };
    // The game resets the slot's per-player state as its client finishes loading, so resend it.
    _fullConnects = slots.FullyConnected += [this](int slot) {
        _playersChangedSinceSpawn = true;
        _written.RemoveSlot(slot);
    };
}

ScreenEntity::~ScreenEntity()
{
    Remove();
}

bool ScreenEntity::Exists() const
{
    return static_cast<bool>(_entities.Get(_entity));
}

bool ScreenEntity::EnsureSpawned(int slot)
{
    if (slot != EveryoneSlot && !IsValidSlot(slot))
    {
        return false;
    }

    const auto target = CacheSlotFor(slot);
    if (!target)
    {
        return false;
    }

    if (!Exists() && !SpawnOrWarn())
    {
        return false;
    }

    if (*target == EveryoneSlot || Covers(*target))
    {
        return true;
    }

    // Respawning for any other reason would retry a hopeless spawn every frame.
    if (!_playersChangedSinceSpawn)
    {
        return false;
    }

    return SpawnOrWarn() && Covers(*target);
}

void ScreenEntity::Remove()
{
    if (IsForPlayer())
    {
        _visibility.ShowToEveryone(_entity);
    }
    _entities.Get(_entity).Remove();

    _entity = {};
    _written.Clear();
}

void ScreenEntity::WriteText(int slot, std::string_view variable, std::string_view value)
{
    const auto target = CacheSlotFor(slot);
    // One entity holds one layout, so the variable alone names the text.
    if (!target || !_written.Changed(*target, WriteKind::Text, {}, variable, value))
    {
        return;
    }

    CheckWrite(*target, SendText(ContentSlotFor(*target), variable, value), variable);
}

void ScreenEntity::WriteClass(int slot, std::string_view elementId, std::string_view className, bool on)
{
    const auto target = CacheSlotFor(slot);
    if (!target || !_written.Changed(*target, WriteKind::Class, elementId, className, on ? "1" : "0"))
    {
        return;
    }

    CheckWrite(*target, SendClass(ContentSlotFor(*target), elementId, className, on), elementId);
}

void ScreenEntity::WriteCursor(int slot, bool shown)
{
    const auto target = CacheSlotFor(slot);
    if (!target || !_written.CursorChanged(*target, shown))
    {
        return;
    }

    CheckWrite(*target, SendCursor(*target, shown), "the cursor");
}

Result<int> ScreenEntity::CacheSlotFor(int slot) const
{
    if (!IsForPlayer())
    {
        return slot;
    }

    if (slot != EveryoneSlot && slot != _owner)
    {
        return std::unexpected(Error::Invalid(std::format("this screen belongs to slot {}", _owner)));
    }

    return _owner;
}

int ScreenEntity::ContentSlotFor(int cacheSlot) const noexcept
{
    return IsForPlayer() ? EveryoneSlot : cacheSlot;
}

Status ScreenEntity::Spawn()
{
    _playersChangedSinceSpawn = false;
    Remove();

    if (Status available = _entities.Available(); !available)
    {
        return available;
    }

    KeyValues values;
    values.Set("layout", _layout.Resource());

    const Entity spawned = _entities.Spawn("custom_hud_layout", values);
    if (!spawned)
    {
        return std::unexpected(Error::Engine("the engine refused to spawn custom_hud_layout"));
    }

    _entity = spawned.Ref();
    if (IsForPlayer())
    {
        _visibility.ShowOnlyTo(_entity, _owner);
    }
    return {};
}

bool ScreenEntity::SpawnOrWarn()
{
    const Status spawned = Spawn();
    if (!spawned)
    {
        Log::Warn("Screen '{}': spawn failed ({}).", _layout.Name(), spawned.error().Detail);
    }
    return spawned.has_value();
}

bool ScreenEntity::Covers(int slot) const
{
    return IsValidSlot(slot) && PlayerStateCount() > slot;
}

void ScreenEntity::CheckWrite(int cacheSlot, const Status& status, std::string_view what)
{
    if (status || !IsValidSlot(cacheSlot))
    {
        return;
    }

    _written.Invalidate(cacheSlot);
    if (_written.IsFirstFailure(cacheSlot))
    {
        Log::Warn("Screen '{}': writing {} for slot {} failed ({}).", _layout.Name(), what, cacheSlot,
                  status.error().Detail);
    }
}

}  // namespace VoltMod
