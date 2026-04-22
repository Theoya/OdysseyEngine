#pragma once
#include "core/types.h"
#include <optional>

namespace odyssey::physics {

// Collider shapes for unified contact generation.
// Note: SphereCollider already exists in collision.h; we re-declare it here
// for consistency with the unified Contact-based interface.

struct BoxCollider {
    vec3 center = {0, 0, 0};
    vec3 half_extents = {0.5f, 0.5f, 0.5f};
};

struct CapsuleCollider {
    vec3 center = {0, 0, 0};
    float radius = 0.4f;
    float height = 1.8f;  // total height; axis is Y
};

struct SphereCollider {
    vec3 center = {0, 0, 0};
    float radius = 0.5f;
};

// Contact data (result of a collision test).
struct Contact {
    vec3 point;          // world space contact point
    vec3 normal;         // unit normal from A toward B
    float penetration;   // overlap depth (> 0 if separated, < 0 if overlap exists)
};

// Pure contact tests. All return std::optional<Contact>;
// if no contact, returns std::nullopt.

// Sphere-sphere collision.
// Derivation: two spheres collide when |pb - pa| < ra + rb.
// Contact normal: n = normalize(pb - pa).
// Contact point: pa + ra * n (surface of sphere a toward b).
// Penetration: (ra + rb - |pb - pa|); positive = overlap, negative = separated.
std::optional<Contact> collide_sphere_sphere(const SphereCollider& a, const vec3& pa,
                                             const SphereCollider& b, const vec3& pb);

// Sphere-box collision.
// Derivation: find the closest point on the box to the sphere center.
// If this point is inside the sphere, we have a contact.
// Normal points from box to sphere.
std::optional<Contact> collide_sphere_box(const SphereCollider& s, const vec3& ps,
                                          const BoxCollider& b, const vec3& pb);

// Sphere-ground (horizontal plane at y = ground_y).
// Derivation: sphere penetrates ground when ps.y - s.radius < ground_y.
// Contact normal always points up (0, 1, 0).
// Contact point is at (ps.x, ground_y, ps.z).
std::optional<Contact> collide_sphere_ground(const SphereCollider& s, const vec3& ps,
                                             float ground_y);

// Capsule-ground collision.
// Derivation: capsule is modeled as a line segment from
// (pc.x, pc.y - height/2, pc.z) to (pc.x, pc.y + height/2, pc.z),
// with radius r around this line. The closest point to ground is the
// point on this line nearest to ground_y, with the sphere radius applied.
std::optional<Contact> collide_capsule_ground(const CapsuleCollider& c, const vec3& pc,
                                              float ground_y);

}  // namespace odyssey::physics
