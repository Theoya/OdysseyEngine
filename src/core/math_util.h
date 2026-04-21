#pragma once

// ---------------------------------------------------------------------------
// math_util.h
// Pure math helpers that aren't big enough to deserve their own subsystem.
// Every function here is const-in / value-out, no side effects, no GPU, no
// filesystem. Safe to unit-test without a graphics context.
//
// Phase 4 (editor): the Inspector authors rotations as XYZ Euler angles
// (degrees) but SceneData stores rotations as quaternions. The round-trip
// must be stable — specifically, the serializer must not produce a different
// rotation quaternion than the one the Inspector showed on load.
//
// Derivation (first-principles per engineering mandate 3):
//
//   A unit quaternion q = (w, x, y, z) represents a rotation by angle 2*acos(w)
//   around the axis (x, y, z)/sin(acos(w)). Euler XYZ convention (our choice,
//   matching glTF and the common game-engine default) decomposes a rotation
//   into three successive rotations around the body-fixed axes:
//
//       R = Rx(roll) * Ry(pitch) * Rz(yaw)
//
//   The corresponding quaternion is the product of the three single-axis
//   quaternions:
//
//       q = qx * qy * qz
//       qx = (cos(r/2), sin(r/2), 0,      0)
//       qy = (cos(p/2), 0,        sin(p/2), 0)
//       qz = (cos(y/2), 0,        0,      sin(y/2))
//
//   Expanded (with cx = cos(r/2), sx = sin(r/2), etc.):
//
//       w = cx*cy*cz - sx*sy*sz
//       x = sx*cy*cz + cx*sy*sz
//       y = cx*sy*cz - sx*cy*sz
//       z = cx*cy*sz + sx*sy*cz
//
//   The inverse (quat → euler) uses atan2 for roll/yaw and asin for pitch.
//   Pitch is clamped to [-pi/2 + eps, pi/2 - eps] to avoid gimbal-lock
//   singularity where roll and yaw become ambiguous.
//
// This file stays header-only; every function is small, branchless outside
// the gimbal-lock clamp, and inlineable.
// ---------------------------------------------------------------------------

#include "core/types.h"

#include <algorithm>
#include <cmath>

namespace odyssey {

// Degrees <-> radians. Value-in, value-out.
inline float deg_to_rad(float d) { return d * 0.01745329251994329577f; } // pi/180
inline float rad_to_deg(float r) { return r * 57.2957795130823208768f; } // 180/pi

// vec3 (euler degrees, XYZ order) -> quat.
//
// Input:  euler_deg = (roll_x, pitch_y, yaw_z) in DEGREES.
// Output: unit quaternion (w, x, y, z) representing the same rotation.
inline quat euler_xyz_deg_to_quat(const vec3& euler_deg) {
    const float rx = deg_to_rad(euler_deg.x) * 0.5f;
    const float ry = deg_to_rad(euler_deg.y) * 0.5f;
    const float rz = deg_to_rad(euler_deg.z) * 0.5f;

    const float cx = std::cos(rx), sx = std::sin(rx);
    const float cy = std::cos(ry), sy = std::sin(ry);
    const float cz = std::cos(rz), sz = std::sin(rz);

    quat q;
    q.w = cx * cy * cz - sx * sy * sz;
    q.x = sx * cy * cz + cx * sy * sz;
    q.y = cx * sy * cz - sx * cy * sz;
    q.z = cx * cy * sz + sx * sy * cz;
    return q;
}

// quat -> vec3 (euler degrees, XYZ order).
//
// Uses atan2 for roll/yaw (unambiguous) and asin for pitch (clamped to
// avoid NaN at the poles). At |pitch| ~ 90deg the decomposition is
// intrinsically ambiguous; the caller should not rely on round-trip
// identity near the singularity. Edit-mode mutations stay well away.
inline vec3 quat_to_euler_xyz_deg(const quat& q) {
    // Normalize for numerical safety — a denormalized quat produces
    // garbage euler angles.
    const float n2 = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;
    float w = q.w, x = q.x, y = q.y, z = q.z;
    if (n2 > 0.0f && std::fabs(n2 - 1.0f) > 1e-6f) {
        const float inv = 1.0f / std::sqrt(n2);
        w *= inv; x *= inv; y *= inv; z *= inv;
    }

    // Inverse of R = Rx(roll) * Ry(pitch) * Rz(yaw). From the rotation
    // matrix derived above (see header comment): R[0][2] = sin(pitch),
    // R[1][2] = -sin(roll)*cos(pitch), R[0][1] = -cos(pitch)*sin(yaw),
    // and the quaternion-to-matrix identities
    //   R[0][2] = 2(xz + wy), R[1][2] = 2(yz - wx), R[0][1] = 2(xy - wz).
    // Substituting gives the three extraction formulas below. The ZYX
    // (aerospace) convention flips the sign on each cross term — those
    // signs were previously wrong and only masked by pure-axis tests
    // where the offending term is zero.

    // roll (x-axis rotation): atan2(-R[1][2], R[2][2]) / cos(pitch)
    const float sinr_cosp = 2.0f * (w * x - y * z);
    const float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    const float roll = std::atan2(sinr_cosp, cosr_cosp);

    // pitch (y-axis rotation): asin(R[0][2]), clamped for singularity.
    float sinp = 2.0f * (w * y + x * z);
    sinp = std::clamp(sinp, -1.0f, 1.0f);
    const float pitch = std::asin(sinp);

    // yaw (z-axis rotation): atan2(-R[0][1], R[0][0]) / cos(pitch)
    const float siny_cosp = 2.0f * (w * z - x * y);
    const float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    const float yaw = std::atan2(siny_cosp, cosy_cosp);

    return vec3{rad_to_deg(roll), rad_to_deg(pitch), rad_to_deg(yaw)};
}

// Convenience: is this quaternion (approximately) the identity rotation?
// Useful as a default-value check in serializers.
inline bool quat_is_identity(const quat& q, float eps = 1e-5f) {
    return std::fabs(q.w - 1.0f) < eps &&
           std::fabs(q.x)        < eps &&
           std::fabs(q.y)        < eps &&
           std::fabs(q.z)        < eps;
}

} // namespace odyssey
