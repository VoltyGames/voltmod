#include "Ui/ScreenEntity.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Entities/Entity.hpp>
#include <VoltMod/Entities/KeyValues.hpp>
#include <string>
#include <utility>

namespace VoltMod
{

/** `<block>--<name>`, built in @p out. */
static std::string_view ModifierClass(std::string& out, std::string_view block, std::string_view name)
{
    out.assign(block).append("--").append(name);
    return out;
}

ScreenEntity::ScreenEntity(EntitySystem& entities, SlotEvents& slots, Visibility& visibility, LayoutPath layout,
                           int owner)
    : _entities(entities), _visibility(visibility), _layout(std::move(layout)), _owner(owner)
{
    _slotChanges = slots.Changed += [this](int slot) {
        _playersChangedSinceSpawn = true;
        if (slot == _owner)  // the screen leaves with its owner
        {
            Remove();
        }
    };
    // The game resets the owner's per-player state, the cursor, as their client finishes loading.
    _fullConnects = slots.FullyConnected += [this](int slot) {
        _playersChangedSinceSpawn = true;
        if (slot == _owner)
        {
            _written.ResetCursor(_owner);
        }
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
    if (!Accepts(slot))
    {
        return false;
    }

    if (!Exists() && !SpawnOrWarn())
    {
        return false;
    }

    if (Covers())
    {
        return true;
    }

    // Respawning for any other reason would retry a hopeless spawn every frame.
    if (!_playersChangedSinceSpawn)
    {
        return false;
    }

    return SpawnOrWarn() && Covers();
}

void ScreenEntity::Remove()
{
    _visibility.ShowToEveryone(_entity);
    _entities.Get(_entity).Remove();

    _entity = {};
    _written.Clear();
}

void ScreenEntity::WriteText(int slot, std::string_view variable, std::string_view value)
{
    // One entity holds one layout, so the variable alone names the text.
    if (!Accepts(slot) || !_written.Changed(_owner, WriteKind::Text, {}, variable, value))
    {
        return;
    }

    CheckWrite(SendText(variable, value), variable);
}

void ScreenEntity::WriteClass(int slot, std::string_view elementId, std::string_view className, bool on)
{
    if (!Accepts(slot) || !_written.ClassChanged(_owner, elementId, className, on))
    {
        return;
    }

    CheckWrite(SendClass(elementId, className, on), elementId);
}

void ScreenEntity::WriteModifier(int slot, std::string_view elementId, std::string_view block,
                                 std::span<const std::string_view> names, std::string_view name)
{
    if (!Accepts(slot))
    {
        return;
    }
    std::string className;
    // One modifier is on at a time, so finding the wanted one on leaves nothing to send.
    if (_written.IsClassOn(_owner, elementId, ModifierClass(className, block, name)))
    {
        return;
    }
    for (std::string_view each : names)
    {
        const bool on = each == name;
        const std::string_view eachClass = ModifierClass(className, block, each);
        // Every off write is a networked entry, so only one that changes something is sent.
        if (on || _written.IsClassOn(_owner, elementId, eachClass))
        {
            WriteClass(_owner, elementId, eachClass, on);
        }
    }
}

void ScreenEntity::WriteCursor(int slot, bool shown)
{
    if (!Accepts(slot) || !_written.CursorChanged(_owner, shown))
    {
        return;
    }

    CheckWrite(SendCursor(shown), "the cursor");
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
    _visibility.ShowOnlyTo(_entity, _owner);
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

bool ScreenEntity::Covers() const
{
    return PlayerStateCount() > _owner;
}

void ScreenEntity::CheckWrite(const Status& status, std::string_view what)
{
    if (status)
    {
        return;
    }

    _written.Invalidate(_owner);
    if (_written.IsFirstFailure(_owner))
    {
        Log::Warn("Screen '{}': writing {} for slot {} failed ({}).", _layout.Name(), what, _owner,
                  status.error().Detail);
    }
}

}  // namespace VoltMod
