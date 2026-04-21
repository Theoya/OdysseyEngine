#pragma once
//
// listener.h — pure listener-relative geometry helpers for voice
// spatialization. Given a listener pose (position + orthonormal basis) and a
// world-space source position, compute the signed azimuth and the Euclidean
// distance the spatializer feeds into the pan law and the distance LPF.
//
// Pure functions only. No I/O, no globals.
//
// Coordinate convention: right-handed, Y-up. `forward` points the way the
// listener is facing, `right` is the listener's right hand, `up` = cross(right, forward)
// (or its negation depending on handedness — we accept whatever the caller
// supplies and only require orthonormality within 1e-3).
//

#include <glm/glm.hpp>

namespace odyssey::audio::voice::dsp {

// ---------------------------------------------------------------------------
// ListenerPose — the minimum state the spatializer needs.
// ---------------------------------------------------------------------------
struct ListenerPose {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 forward {0.0f, 0.0f, -1.0f}; // listener's forward, unit vector
    glm::vec3 right   {1.0f, 0.0f, 0.0f};  // listener's right, unit vector
    glm::vec3 up      {0.0f, 1.0f, 0.0f};  // listener's up, unit vector
};

// ---------------------------------------------------------------------------
// source_distance — Euclidean distance from listener to source in meters.
// World units are meters in OdysseyEngine (Nadir + physics convention).
// ---------------------------------------------------------------------------
float source_distance(const ListenerPose& listener, const glm::vec3& source_world) noexcept;

// ---------------------------------------------------------------------------
// source_azimuth — signed angle in radians in the horizontal plane.
//
// Derivation:
//   let p = source - listener, expressed in listener-local basis:
//     p_forward = dot(p, forward)    (how far in front)
//     p_right   = dot(p, right)      (how far to the right, signed)
//   θ = atan2(p_right, p_forward)
//
// Range: (-π, +π]. Positive = right of listener, negative = left.
// Matches the convention used by the equal-power pan law in spatializer.cpp.
//
// For co-located source (|p| == 0) returns 0 by convention — callers should
// short-circuit via source_distance before panning.
// ---------------------------------------------------------------------------
float source_azimuth(const ListenerPose& listener, const glm::vec3& source_world) noexcept;

// ---------------------------------------------------------------------------
// is_behind_listener — returns true if source is in the rear hemisphere
// (|θ| > π/2). The spatializer uses this to apply the -3 dB rear
// attenuation and the 1 kHz front/back cue.
// ---------------------------------------------------------------------------
bool is_behind_listener(const ListenerPose& listener, const glm::vec3& source_world) noexcept;

} // namespace odyssey::audio::voice::dsp
