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
    return AngleToForward(QAngle(0.0f, yaw, 0.0f));
}

void AngleToAxes(const QAngle& angles, Vector& forward, Vector& left, Vector& up)
{
    Vector right;
    AngleVectors(angles, &forward, &right, &up);
    left = -right;
}

Vector OffsetLocal(const Vector& origin, const QAngle& angles, const Vector& forwardLeftUp)
{
    Vector forward;
    Vector left;
    Vector up;
    AngleToAxes(angles, forward, left, up);
    return origin + forward * forwardLeftUp.x + left * forwardLeftUp.y + up * forwardLeftUp.z;
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
