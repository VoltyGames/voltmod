#pragma once

#include <VoltMod/Engine/EntityRef.hpp>
#include <VoltMod/Entities/Pawn.hpp>

namespace VoltMod
{

/**
 * @brief @ref Pawn::HoldFire that can be let go: a release hands the weapon back the wait it had, so
 * the next click fires at once. Keep one per player.
 */
class FireHold
{
public:
    /** Keep @p pawn's weapon in hand from firing, as @ref Pawn::HoldFire does; call it every frame to hold. */
    void Hold(const Pawn& pawn, const Clock& clock);

    /** Lets the held weapon fire again at its own pace, then forgets it. */
    void Release(const Pawn& pawn);

private:
    EntityRef _weapon;
    /** The weapon's own next attack ticks, from before the hold. */
    int _primary = 0;
    int _secondary = 0;
    /** 0 when not holding. */
    int _until = 0;
};

}  // namespace VoltMod
