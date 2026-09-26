#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Engine/EntityRef.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Engine/Math.hpp>
#include <VoltMod/Entities/Pawn.hpp>

namespace VoltMod
{

/** Which interaction layers a trace stops at. */
enum class TraceLayers
{
    Sight,     ///< what hides one player from another
    Solid,     ///< what a player body collides with, players included
    Surfaces,  ///< every drawn surface and the sky; no player clips or players
};

struct TraceOptions
{
    TraceLayers Layers = TraceLayers::Sight;
    Entity Ignore1;
    Entity Ignore2;
    /** Also skips everything this entity owns; see @ref PropSpec::Owner. */
    Entity IgnoreOwnedBy;
};

struct TraceHit
{
    bool Hit = false;       ///< also true when the start is inside a solid
    float Fraction = 1.0f;  ///< share of the path travelled
    Vector End;
    Vector Normal;          ///< zero without a hit
    EntityRef HitEntity;    ///< the world included
    bool HitWorld = false;
};

/**
 * @brief `runtime.Trace`: line and box traces against the physics world. Game-thread only.
 *
 * @code
 * const auto clear = runtime.Trace.Clear(eye, target, {.Ignore1 = self, .Ignore2 = other});
 * if (clear && *clear)
 *     ...
 * @endcode
 */
class Trace
{
public:
    /** @p bindings must outlive this service. */
    explicit Trace(const Bindings& bindings) : _bindings(bindings) {}
    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;

    /** Unsupported when the Nav_TraceLine slot did not bind. */
    Status Available() const;

    Result<TraceHit> Line(const Vector& from, const Vector& to, const TraceOptions& options = {}) const;

    /** Sweeps @p mins..@p maxs along the path; `End` is the box origin. Unsupported when the
     *  Nav_TraceShape slot did not bind. */
    Result<TraceHit> Box(const Vector& from, const Vector& to, const Vector& mins, const Vector& maxs,
                         const TraceOptions& options = {}) const;

    /** Along @p pawn's aim, ignoring the pawn. */
    Result<TraceHit> FromEyes(const Pawn& pawn, float distance, TraceLayers layers = TraceLayers::Solid) const;

    Result<bool> Clear(const Vector& from, const Vector& to, const TraceOptions& options = {}) const;

private:
    const Bindings& _bindings;
};

}  // namespace VoltMod
