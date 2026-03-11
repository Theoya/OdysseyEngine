#pragma once
#include "physics/collision.h"
#include "physics/mesh_collider.h"
#include <vector>

namespace odyssey::physics {

class CollisionSystem {
public:
    uint32_t add_ground_plane(const GroundPlane& plane);
    uint32_t add_mesh_collider(MeshCollider collider);

    // Query the ground height at a world XZ position (checks planes + mesh colliders)
    float ground_height_at(float x, float z) const;

    // Cast a ray against all colliders
    bool raycast(const Ray& ray, RayHit& hit, float max_dist = 1000.0f) const;

private:
    std::vector<GroundPlane> ground_planes_;
    std::vector<MeshCollider> mesh_colliders_;
};

} // namespace odyssey::physics
