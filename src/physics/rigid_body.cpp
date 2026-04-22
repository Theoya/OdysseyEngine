#include "physics/rigid_body.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace odyssey::physics {

RigidBody integrate(const RigidBody& body, const vec3& force_applied,
                    const vec3& torque_applied, const vec3& gravity, float dt) {
    RigidBody result = body;

    // Kinematic bodies are moved by their velocity but not affected by forces or gravity.
    if (body.is_kinematic) {
        result.position = body.position + body.velocity * dt;
        return result;
    }

    // Static bodies (mass == 0) cannot move.
    if (body.mass <= 0.0f) {
        result.velocity = {0, 0, 0};
        result.angular_velocity = {0, 0, 0};
        return result;
    }

    // Semi-implicit Euler: apply forces to velocity first, then update position.
    // Total force: applied force + gravity + linear drag (opposing velocity).
    vec3 total_force = force_applied;
    if (body.use_gravity) {
        total_force += body.mass * gravity;
    }

    // Linear drag: F_drag = -drag * mass * v
    // This is a velocity-dependent damping term.
    total_force -= body.drag * body.mass * body.velocity;

    // Acceleration: a = F / m
    vec3 acceleration = total_force / body.mass;

    // Update velocity (semi-implicit step 1).
    result.velocity = body.velocity + acceleration * dt;

    // Update position using the new velocity (semi-implicit step 2).
    result.position = body.position + result.velocity * dt;

    // Angular dynamics: apply angular damping.
    // ω' = ω * (1 - angular_drag*dt)
    // Clamp the damping factor to [0, 1] to avoid instability.
    float angular_damping_factor = glm::max(0.0f, 1.0f - body.angular_drag * dt);
    result.angular_velocity = body.angular_velocity * angular_damping_factor;

    // Update orientation by integrating angular velocity.
    // Derivation: the quaternion derivative is q' = (ω_quat * q) / 2,
    // where ω_quat = (0, ω_x, ω_y, ω_z) is the angular velocity as a quaternion.
    // Over a small time step dt, we integrate: q' ≈ q + q' * dt.
    if (glm::length(result.angular_velocity) > 1e-6f) {
        quat omega_quat = {0.0f, result.angular_velocity.x, result.angular_velocity.y,
                           result.angular_velocity.z};
        quat dq = (omega_quat * body.orientation) * (0.5f * dt);
        result.orientation = glm::normalize(body.orientation + dq);
    }

    return result;
}

glm::mat3 sphere_inertia(float mass, float radius) {
    // For a solid sphere: I = (2/5) * m * r^2
    // All diagonal elements are equal, off-diagonals are zero.
    float I = (2.0f / 5.0f) * mass * radius * radius;
    return glm::mat3(I, 0, 0,
                     0, I, 0,
                     0, 0, I);
}

glm::mat3 box_inertia(float mass, const vec3& half_extents) {
    // For a box with half-extents (a, b, c):
    // I_xx = (m/3) * (b^2 + c^2)
    // I_yy = (m/3) * (a^2 + c^2)
    // I_zz = (m/3) * (a^2 + b^2)
    float a2 = half_extents.x * half_extents.x;
    float b2 = half_extents.y * half_extents.y;
    float c2 = half_extents.z * half_extents.z;
    float factor = mass / 3.0f;

    float Ixx = factor * (b2 + c2);
    float Iyy = factor * (a2 + c2);
    float Izz = factor * (a2 + b2);

    return glm::mat3(Ixx, 0, 0,
                     0, Iyy, 0,
                     0, 0, Izz);
}

}  // namespace odyssey::physics
