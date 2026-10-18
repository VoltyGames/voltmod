#pragma once

#include <VoltMod/Engine/EntityRef.hpp>
#include <VoltMod/Entities/Entity.hpp>
#include <VoltMod/Entities/Pawn.hpp>
#include <VoltMod/Schema/Generated/Enums.hpp>
#include <cstdint>

namespace VoltMod
{

/**
 * @brief Has one pawn copy another entity's bones while traces, bullets included, keep hitting it,
 * and gives that pawn alone its collision back on detach, so a respawn is never written to. Keep
 * one per player.
 */
class BoneFollow
{
public:
    /** Parent @p pawn to @p parent with its bones taking the parent's pose by name. Does nothing
     *  when a pawn already follows, or when either is gone. */
    void Attach(const Pawn& pawn, const Entity& parent);

    /** Clear @p pawn's parent and give back its collision if it is the one following, then forget it. */
    void Detach(const Pawn& pawn);

    /** Whether a pawn is following. */
    explicit operator bool() const noexcept { return static_cast<bool>(_pawn); }

private:
    EntityRef _pawn;
    uint8_t _mask = 0;
    Schema::SurroundingBoundsType_t _surround{};
};

}  // namespace VoltMod
