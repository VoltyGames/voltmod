#include <VoltMod/Engine/Math.hpp>
#include <algorithm>
#include <cmath>
#include <mathlib/mathlib.h>

namespace VoltMod
{

Vector AngleToForward(const QAngle& angles)
{
    Vector forward;
    AngleVectors(angles, &forward);
    return forward;
}

Vector YawToForward(float yaw)
{
    return {std::cos(yaw / DegreesPerRadian), std::sin(yaw / DegreesPerRadian), 0.0f};
}

QAngle Axes::ToAngles() const
{
    return {-std::asin(std::clamp(Forward.z, -1.0f, 1.0f)) * DegreesPerRadian,
            std::atan2(Forward.y, Forward.x) * DegreesPerRadian, std::atan2(Left.z, Up.z) * DegreesPerRadian};
}

Axes AngleToAxes(const QAngle& angles)
{
    Axes axes;
    Vector right;
    AngleVectors(angles, &axes.Forward, &right, &axes.Up);
    axes.Left = -right;
    return axes;
}

Vector OffsetLocal(const Vector& origin, const QAngle& angles, const Vector& forwardLeftUp)
{
    const Axes axes = AngleToAxes(angles);
    return origin + axes.Forward * forwardLeftUp.x + axes.Left * forwardLeftUp.y + axes.Up * forwardLeftUp.z;
}

QAngle AngleTo(const Vector& from, const Vector& to)
{
    const Vector d = to - from;
    const float across = std::sqrt(d.x * d.x + d.y * d.y);
    return {-std::atan2(d.z, across) * DegreesPerRadian, std::atan2(d.y, d.x) * DegreesPerRadian, 0.0f};
}

float AngleDelta(float from, float to)
{
    const float diff = std::fmod(to - from + 180.0f, 360.0f);
    return (diff < 0.0f ? diff + 360.0f : diff) - 180.0f;
}

float TurnToward(float current, float target, float maxStep)
{
    return current + std::clamp(AngleDelta(current, target), -maxStep, maxStep);
}

}  // namespace VoltMod
