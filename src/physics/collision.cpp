#include "physics/collision.h"
#include <glm/geometric.hpp>
#include <cmath>
#include <algorithm>

namespace odyssey::physics {

std::optional<float> ray_vs_plane(const Ray& ray, const vec3& plane_point, const vec3& plane_normal) {
    float denom = glm::dot(ray.direction, plane_normal);
    // Ray is parallel to the plane
    if (std::abs(denom) < 1e-6f) {
        return std::nullopt;
    }
    float t = glm::dot(plane_point - ray.origin, plane_normal) / denom;
    if (t >= 0.0f) {
        return t;
    }
    return std::nullopt;
}

std::optional<RayHit> ray_vs_sphere(const Ray& ray, const SphereCollider& sphere) {
    vec3 oc = ray.origin - sphere.center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return std::nullopt;
    }

    float sqrt_disc = std::sqrt(discriminant);
    float inv_2a = 1.0f / (2.0f * a);

    // Try the closer intersection first
    float t = (-b - sqrt_disc) * inv_2a;
    if (t < 0.0f) {
        // If the closer hit is behind us, try the farther one
        t = (-b + sqrt_disc) * inv_2a;
        if (t < 0.0f) {
            return std::nullopt;
        }
    }

    RayHit hit;
    hit.point = ray.origin + ray.direction * t;
    hit.normal = glm::normalize(hit.point - sphere.center);
    hit.distance = t;
    hit.triangle_index = 0;
    return hit;
}

std::optional<RayHit> ray_vs_aabb(const Ray& ray, const AABB& aabb) {
    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();
    vec3 hit_normal{0.0f};

    for (int i = 0; i < 3; ++i) {
        if (std::abs(ray.direction[i]) < 1e-8f) {
            // Ray is parallel to slab — check if origin is inside
            if (ray.origin[i] < aabb.min[i] || ray.origin[i] > aabb.max[i]) {
                return std::nullopt;
            }
        } else {
            float inv_d = 1.0f / ray.direction[i];
            float t1 = (aabb.min[i] - ray.origin[i]) * inv_d;
            float t2 = (aabb.max[i] - ray.origin[i]) * inv_d;

            // Track which face we enter through for the normal
            vec3 normal_at_t1{0.0f};
            normal_at_t1[i] = (inv_d < 0.0f) ? 1.0f : -1.0f;

            if (t1 > t2) {
                std::swap(t1, t2);
                normal_at_t1[i] = -normal_at_t1[i];
            }

            if (t1 > tmin) {
                tmin = t1;
                hit_normal = normal_at_t1;
            }
            tmax = std::min(tmax, t2);

            if (tmin > tmax) {
                return std::nullopt;
            }
        }
    }

    // If tmin is negative, the ray starts inside the AABB
    float t = tmin;
    if (t < 0.0f) {
        t = tmax;
        if (t < 0.0f) {
            return std::nullopt;
        }
        // Inside the box — normal points away from the exit face
        hit_normal = -hit_normal;
    }

    RayHit hit;
    hit.point = ray.origin + ray.direction * t;
    hit.normal = hit_normal;
    hit.distance = t;
    hit.triangle_index = 0;
    return hit;
}

bool sphere_vs_sphere(const SphereCollider& a, const SphereCollider& b) {
    float dist = glm::length(a.center - b.center);
    return dist < (a.radius + b.radius);
}

std::optional<float> sphere_vs_plane(const SphereCollider& sphere, const vec3& plane_point, const vec3& plane_normal) {
    float signed_dist = glm::dot(sphere.center - plane_point, plane_normal);
    if (std::abs(signed_dist) < sphere.radius) {
        return signed_dist;
    }
    return std::nullopt;
}

} // namespace odyssey::physics
