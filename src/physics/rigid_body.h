#pragma once
#include "core/types.h"
#include <glm/glm.hpp>

namespace odyssey::physics {

// Rigid body with linear and angular dynamics.
// Integrates using semi-implicit (symplectic) Euler for stability.
struct RigidBody {
    vec3 position = {0, 0, 0};
    vec3 velocity = {0, 0, 0};
    vec3 angular_velocity = {0, 0, 0};
    quat orientation = {1, 0, 0, 0};  // identity
    float mass = 1.0f;                // kg; 0 = infinite (static)
    float drag = 0.01f;               // linear damping coefficient per second
    float angular_drag = 0.05f;       // angular damping coefficient per second
    bool use_gravity = true;
    bool is_kinematic = false;  // kinematic = moved by code, not by forces
};

// Integrate one time step using semi-implicit (symplectic) Euler.
// Derivation: semi-implicit Euler is more stable than explicit Euler for
// constant acceleration + damping at typical game step sizes (dt = 1/60 s).
// With explicit Euler, the iteration x' = x + v*dt; v' = v + a*dt can exhibit
// energy growth for oscillators (unstable). Semi-implicit applies the
// acceleration first: v' = v + a*dt, then uses the updated velocity for
// position: x' = x + v'*dt. This is equivalent to backward Euler and is stable.
//
// Applied forces:
//   F_total = F_applied + (mass * gravity) - (drag * mass * v)
//   a = F_total / mass (if mass > 0; kinematic bodies ignore forces)
//   v' = v + a*dt
//   x' = x + v'*dt
//
// For rotation (ignoring torque in this version, using only angular_drag):
//   ω' = ω * (1 - angular_drag*dt)
//   θ' = integrate quaternion by (ω' * dt / 2)
RigidBody integrate(const RigidBody& body, const vec3& force_applied,
                    const vec3& torque_applied, const vec3& gravity, float dt);

// Compute the world-space inertia tensor for a sphere.
// Derivation: for a solid sphere of mass m and radius r,
// I = (2/5)*m*r^2, so all diagonal elements are equal and off-diagonals are zero.
glm::mat3 sphere_inertia(float mass, float radius);

// Compute the world-space inertia tensor for a box.
// Derivation: for a box with half-extents (a, b, c) and mass m,
// I_xx = (m/3) * (b^2 + c^2), I_yy = (m/3) * (a^2 + c^2), I_zz = (m/3) * (a^2 + b^2).
glm::mat3 box_inertia(float mass, const vec3& half_extents);

}  // namespace odyssey::physics
