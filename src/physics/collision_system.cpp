#include "physics/collision_system.h"
#include <algorithm>
#include <limits>

namespace odyssey::physics {

uint32_t CollisionSystem::add_ground_plane(const GroundPlane& plane) {
    uint32_t index = static_cast<uint32_t>(ground_planes_.size());
    ground_planes_.push_back(plane);
    return index;
}

uint32_t CollisionSystem::add_mesh_collider(MeshCollider collider) {
    uint32_t index = static_cast<uint32_t>(mesh_colliders_.size());
    mesh_colliders_.push_back(std::move(collider));
    return index;
}

float CollisionSystem::ground_height_at(float x, float z) const {
    float max_height = 0.0f;
    bool found = false;

    // Check ground planes
    for (const auto& plane : ground_planes_) {
        // For a plane defined by point P and normal N, the height at (x, z) is:
        // y = P.y + (dot(N, P) - N.x*x - N.z*z) / N.y
        // But only if the normal has a non-zero Y component
        if (std::abs(plane.normal.y) > 1e-6f) {
            float height = plane.point.y +
                (plane.normal.x * (plane.point.x - x) +
                 plane.normal.z * (plane.point.z - z)) / plane.normal.y;
            if (!found || height > max_height) {
                max_height = height;
                found = true;
            }
        }
    }

    // Check mesh colliders
    for (const auto& mesh : mesh_colliders_) {
        float height = 0.0f;
        if (mesh.height_at(x, z, height)) {
            if (!found || height > max_height) {
                max_height = height;
                found = true;
            }
        }
    }

    return max_height;
}

bool CollisionSystem::raycast(const Ray& ray, RayHit& hit, float max_dist) const {
    float closest_dist = max_dist;
    bool found = false;

    for (const auto& mesh : mesh_colliders_) {
        RayHit candidate;
        if (mesh.raycast(ray, candidate, closest_dist)) {
            if (candidate.distance < closest_dist) {
                closest_dist = candidate.distance;
                hit = candidate;
                found = true;
            }
        }
    }

    return found;
}

} // namespace odyssey::physics
