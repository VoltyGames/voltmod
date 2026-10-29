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
    const float sp = std::sin(angles.x / DegreesPerRadian);
    const float cp = std::cos(angles.x / DegreesPerRadian);
    const float sy = std::sin(angles.y / DegreesPerRadian);
    const float cy = std::cos(angles.y / DegreesPerRadian);
    const float sr = std::sin(angles.z / DegreesPerRadian);
    const float cr = std::cos(angles.z / DegreesPerRadian);
    forward = Vector(cp * cy, cp * sy, -sp);
    left = Vector(sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp);
    up = Vector(cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp);
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
