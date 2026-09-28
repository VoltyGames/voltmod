#pragma once

#include <cstdint>

namespace VoltMod
{

/** An entity handle that is safe to keep across frames: it resolves to nothing once its entity is gone,
 *  even if another entity reuses the slot. `EntityRef{}` clears a handle field. */
struct EntityRef
{
    static constexpr uint32_t Unset = 0xFFFFFFFFu;

    /** The engine's CHandle bits. */
    uint32_t Handle = Unset;

    explicit operator bool() const noexcept { return Handle != Unset; }
    bool operator==(const EntityRef&) const noexcept = default;
};

// Generated accessors read and write it in place of the engine's CHandle.
static_assert(sizeof(EntityRef) == 4);

}  // namespace VoltMod
