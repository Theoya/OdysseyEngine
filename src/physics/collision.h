#pragma once
#include "core/types.h"
#include <optional>

namespace odyssey::physics {

struct Ray {
    vec3 origin;
    vec3 direction;  // should be normalized
};

struct RayHit {
    vec3 point;
    vec3 normal;
    float distance;
    uint32_t triangle_index;
};

struct SphereCollider {
    vec3 center;
    float radius;
};

struct AABB {
    vec3 min;
    vec3 max;
};

struct GroundPlane {
    vec3 point{0.0f, 0.0f, 0.0f};
    vec3 normal{0.0f, 1.0f, 0.0f};
};

// Pure collision test functions
std::optional<float> ray_vs_plane(const Ray& ray, const vec3& plane_point, const vec3& plane_normal);
std::optional<RayHit> ray_vs_sphere(const Ray& ray, const SphereCollider& sphere);
std::optional<RayHit> ray_vs_aabb(const Ray& ray, const AABB& aabb);
bool sphere_vs_sphere(const SphereCollider& a, const SphereCollider& b);
std::optional<float> sphere_vs_plane(const SphereCollider& sphere, const vec3& plane_point, const vec3& plane_normal);

} // namespace odyssey::physics
