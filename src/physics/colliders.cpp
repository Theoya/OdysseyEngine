#include "physics/colliders.h"
#include <glm/glm.hpp>
#include <cmath>

namespace odyssey::physics {

std::optional<Contact> collide_sphere_sphere(const SphereCollider& a, const vec3& pa,
                                             const SphereCollider& b, const vec3& pb) {
    vec3 delta = pb - pa;
    float distance = glm::length(delta);
    float min_dist = a.radius + b.radius;

    // Spheres are separated if distance >= min_dist.
    if (distance >= min_dist) {
        return std::nullopt;
    }

    // Avoid division by zero for coincident centers.
    if (distance < 1e-6f) {
        // Spheres are coincident; use an arbitrary normal.
        Contact c;
        c.normal = {0.0f, 1.0f, 0.0f};
        c.point = pa;
        c.penetration = min_dist;
        return c;
    }

    vec3 normal = glm::normalize(delta);
    Contact c;
    c.normal = normal;
    c.point = pa + a.radius * normal;
    c.penetration = min_dist - distance;
    return c;
}

std::optional<Contact> collide_sphere_box(const SphereCollider& s, const vec3& ps,
                                          const BoxCollider& b, const vec3& pb) {
    // Find the closest point on the box to the sphere center.
    vec3 box_min = pb - b.half_extents;
    vec3 box_max = pb + b.half_extents;

    // Clamp sphere center to box bounds.
    vec3 closest = glm::clamp(ps, box_min, box_max);
    vec3 to_center = ps - closest;
    float distance = glm::length(to_center);

    // No collision if sphere is outside the box by more than its radius.
    if (distance > s.radius) {
        return std::nullopt;
    }

    // Collision found.
    Contact c;
    if (distance < 1e-6f) {
        // Sphere center is very close to box surface; use arbitrary normal.
        c.normal = {0.0f, 1.0f, 0.0f};
    } else {
        c.normal = glm::normalize(to_center);
    }
    c.point = closest;
    c.penetration = s.radius - distance;
    return c;
}

std::optional<Contact> collide_sphere_ground(const SphereCollider& s, const vec3& ps,
                                             float ground_y) {
    // Sphere collides with ground when ps.y - s.radius <= ground_y.
    float sphere_bottom = ps.y - s.radius;

    if (sphere_bottom > ground_y) {
        return std::nullopt;  // No collision (separated).
    }

    Contact c;
    c.normal = {0.0f, 1.0f, 0.0f};  // Ground normal points up.
    c.point = {ps.x, ground_y, ps.z};
    c.penetration = ground_y - sphere_bottom;
    return c;
}

std::optional<Contact> collide_capsule_ground(const CapsuleCollider& c, const vec3& pc,
                                              float ground_y) {
    // Capsule is a cylinder with hemispherical caps.
    // Model as a line segment from (pc.x, pc.y - height/2, pc.z) to
    // (pc.x, pc.y + height/2, pc.z), with radius r.
    // The closest point on this line to the ground is
    // (pc.x, clamp(pc.y, ground_y, pc.y + height/2), pc.z).

    float half_height = c.height * 0.5f;
    float capsule_bottom = pc.y - half_height;
    float capsule_top = pc.y + half_height;

    // Find the closest point on the capsule axis to the ground plane.
    float closest_y = glm::clamp(ground_y, capsule_bottom, capsule_top);

    // The closest point on the capsule surface is offset from this axis point
    // by the capsule radius in the downward direction (toward ground).
    float surface_y = closest_y - c.radius;

    if (surface_y > ground_y) {
        return std::nullopt;  // No collision (separated).
    }

    Contact ct;
    ct.normal = {0.0f, 1.0f, 0.0f};
    ct.point = {pc.x, ground_y, pc.z};
    ct.penetration = ground_y - surface_y;
    return ct;
}

}  // namespace odyssey::physics
