#include "physics/physics_world.h"
#include "physics/contact_solver.h"

namespace odyssey::physics {

uint32_t PhysicsWorld::add_body(const RigidBody& body) {
    uint32_t id = static_cast<uint32_t>(bodies_.size());
    bodies_.push_back(body);
    return id;
}

uint32_t PhysicsWorld::add_sphere_collider(uint32_t body_id,
                                           const SphereCollider& collider) {
    uint32_t id = static_cast<uint32_t>(colliders_.size());
    ColliderRef ref;
    ref.body_id = body_id;
    ref.type = ColliderRef::SPHERE;
    ref.sphere = collider;
    colliders_.push_back(ref);
    return id;
}

uint32_t PhysicsWorld::add_capsule_collider(uint32_t body_id,
                                            const CapsuleCollider& collider) {
    uint32_t id = static_cast<uint32_t>(colliders_.size());
    ColliderRef ref;
    ref.body_id = body_id;
    ref.type = ColliderRef::CAPSULE;
    ref.capsule = collider;
    colliders_.push_back(ref);
    return id;
}

uint32_t PhysicsWorld::add_box_collider(uint32_t body_id, const BoxCollider& collider) {
    uint32_t id = static_cast<uint32_t>(colliders_.size());
    ColliderRef ref;
    ref.body_id = body_id;
    ref.type = ColliderRef::BOX;
    ref.box = collider;
    colliders_.push_back(ref);
    return id;
}

const RigidBody& PhysicsWorld::body(uint32_t id) const {
    return bodies_.at(id);
}

RigidBody& PhysicsWorld::body_mut(uint32_t id) {
    return bodies_.at(id);
}

size_t PhysicsWorld::body_count() const {
    return bodies_.size();
}

void PhysicsWorld::step(float dt) {
    // Step 1: Integrate all bodies (gravity is applied inside integrate()).
    for (auto& body : bodies_) {
        body = integrate(body, vec3(0.0f), vec3(0.0f), gravity, dt);
    }

    // Step 2: Collide each collider with the ground and resolve contacts.
    for (const auto& collider_ref : colliders_) {
        if (collider_ref.body_id >= bodies_.size()) {
            continue;  // Invalid body reference.
        }

        RigidBody& body = bodies_[collider_ref.body_id];
        std::optional<Contact> contact_opt;

        // Perform the appropriate collision test.
        switch (collider_ref.type) {
            case ColliderRef::SPHERE:
                contact_opt =
                    collide_sphere_ground(collider_ref.sphere, body.position, ground_y);
                break;
            case ColliderRef::CAPSULE:
                contact_opt =
                    collide_capsule_ground(collider_ref.capsule, body.position, ground_y);
                break;
            case ColliderRef::BOX: {
                // For simplicity, we treat the box as a bounding sphere with the
                // largest half-extent as the radius. This is a conservative approximation.
                SphereCollider approx_sphere;
                approx_sphere.center = collider_ref.box.center;
                float max_extent = collider_ref.box.half_extents.x;
                if (collider_ref.box.half_extents.y > max_extent) {
                    max_extent = collider_ref.box.half_extents.y;
                }
                if (collider_ref.box.half_extents.z > max_extent) {
                    max_extent = collider_ref.box.half_extents.z;
                }
                approx_sphere.radius = max_extent;
                contact_opt = collide_sphere_ground(approx_sphere, body.position, ground_y);
                break;
            }
        }

        // If contact found, resolve it.
        if (contact_opt) {
            Contact& contact = contact_opt.value();

            // Position correction to prevent penetration accumulation.
            PositionCorrection pos_corr =
                correct_positions(body, nullptr, contact, 0.01f, 0.2f);
            body.position += pos_corr.delta_a;

            // Impulse resolution.
            SolverInput solver_input;
            solver_input.body_a = &body;
            solver_input.body_b = nullptr;  // Ground is static.
            solver_input.contact = contact;
            solver_input.restitution = restitution;
            solver_input.friction = friction;

            SolverOutput solver_output = solve_contact(solver_input);
            body.velocity += solver_output.impulse_a / body.mass;
        }
    }
}

}  // namespace odyssey::physics
