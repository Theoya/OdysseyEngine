#pragma once
#include "physics/rigid_body.h"
#include "physics/colliders.h"
#include <vector>
#include <cstdint>

namespace odyssey::physics {

// PhysicsWorld manages a collection of rigid bodies and colliders,
// stepping them forward with gravity and contact resolution.
class PhysicsWorld {
public:
    PhysicsWorld() = default;

    // Add a body and return its handle.
    uint32_t add_body(const RigidBody& body);

    // Add colliders to a body (referenced by body_id).
    uint32_t add_sphere_collider(uint32_t body_id, const SphereCollider& collider);
    uint32_t add_capsule_collider(uint32_t body_id, const CapsuleCollider& collider);
    uint32_t add_box_collider(uint32_t body_id, const BoxCollider& collider);

    // Step the simulation by dt (seconds).
    // Performs: gravity application -> integrate -> collide with ground -> impulse solve.
    void step(float dt);

    // Read-only accessors.
    const RigidBody& body(uint32_t id) const;
    RigidBody& body_mut(uint32_t id);
    size_t body_count() const;

    // Configuration.
    vec3 gravity = {0.0f, -9.81f, 0.0f};
    float ground_y = 0.0f;
    float restitution = 0.1f;  // bounciness
    float friction = 0.5f;     // friction coefficient

private:
    struct ColliderRef {
        uint32_t body_id = 0;
        enum Type { SPHERE, CAPSULE, BOX } type = SPHERE;
        SphereCollider sphere;
        CapsuleCollider capsule;
        BoxCollider box;
    };

    std::vector<RigidBody> bodies_;
    std::vector<ColliderRef> colliders_;
};

}  // namespace odyssey::physics
