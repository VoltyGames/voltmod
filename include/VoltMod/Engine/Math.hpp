#pragma once

// Include this for Vector and QAngle instead of the SDK's <mathlib/vector.h>.
#include <mathlib/vector.h>
#include <numbers>

namespace VoltMod
{

/** The engine's Vector. Its default constructor leaves the value uninitialised; write
 *  `Vector{0.0f, 0.0f, 0.0f}` for a zero. */
using Vector = ::Vector;

/** The engine's angles in degrees: (pitch, yaw, roll), positive pitch looks down. */
using QAngle = ::QAngle;

inline constexpr float DegreesPerRadian = 180.0f / std::numbers::pi_v<float>;

/** The unit vector @p angles aim along; roll does not change it. */
Vector AngleToForward(const QAngle& angles);

/** The level unit vector heading @p yaw degrees. */
Vector YawToForward(float yaw);

/** A body's unit forward, left and up vectors. Left is the engine's right axis reversed. */
struct Axes
{
    Vector Forward{1.0f, 0.0f, 0.0f};
    Vector Left{0.0f, 1.0f, 0.0f};
    Vector Up{0.0f, 0.0f, 1.0f};

    /** As engine angles, with pitch and yaw in [-180, 180]. */
    QAngle ToAngles() const;
};

/** The axes of a body turned to @p angles. */
Axes AngleToAxes(const QAngle& angles);

/** The point @p forwardLeftUp away from @p origin along the axes of @p angles, such as a muzzle on a
 *  turret that turns and pitches about @p origin. */
Vector OffsetLocal(const Vector& origin, const QAngle& angles, const Vector& forwardLeftUp);

/** The angles that aim from @p from at @p to, with no roll. Pitch is in [-90, 90], positive down, as
 *  the engine has it. */
QAngle AngleTo(const Vector& from, const Vector& to);

/** How far @p to is from @p from in degrees, the short way round, in [-180, 180). */
float AngleDelta(float from, float to);

/** @p current turned toward @p target by at most @p maxStep degrees, the short way round. */
float TurnToward(float current, float target, float maxStep);

}  // namespace VoltMod
