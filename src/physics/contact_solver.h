#pragma once
#include "physics/rigid_body.h"
#include "physics/colliders.h"

namespace odyssey::physics {

// Input to the contact solver.
struct SolverInput {
    const RigidBody* body_a;  // non-null
    const RigidBody* body_b;  // may be null for collisions with static ground
    Contact contact;
    float restitution = 0.0f;  // 0 = fully damped, 1 = perfectly elastic
    float friction = 0.5f;     // Coulomb friction coefficient
};

// Output from the contact solver.
struct SolverOutput {
    vec3 impulse_a;  // applied as a.velocity += impulse_a / mass_a
    vec3 impulse_b;  // applied as b.velocity += impulse_b / mass_b
};

// Solve a single contact using impulse-based resolution with Coulomb friction.
// Derivation: the relative velocity in the normal direction is
//   v_rel_n = dot(v_b - v_a, normal).
// The impulse magnitude to stop this relative motion is
//   j_n = -(1 + e) * v_rel_n / (1/m_a + 1/m_b),
// where e is the coefficient of restitution. If body_b is null (infinite mass),
// the term 1/m_b = 0. The impulse is applied as
//   impulse = j_n * normal.
//
// Tangential friction impulse is computed along the tangent direction
// perpendicular to the normal, clamped by the Coulomb friction cone:
//   |j_t| <= mu * |j_n|.
SolverOutput solve_contact(const SolverInput& in);

// Position correction using Baumgarte stabilization.
// Derivation: penetration in rigid body simulation can accumulate due to
// discrete time stepping. Baumgarte stabilization adds a velocity correction
// to push overlapping bodies apart:
//   v_correction = beta * max(0, penetration - slop) / dt.
// This is applied as a positional offset directly:
//   delta_a = -v_correction * dt / (1/m_a + 1/m_b) for body a,
//   delta_b =  v_correction * dt / (1/m_a + 1/m_b) for body b.
// Typically beta ~ 0.2 and slop ~ 0.01 m.
struct PositionCorrection {
    vec3 delta_a;  // position change for body a
    vec3 delta_b;  // position change for body b
};

PositionCorrection correct_positions(const RigidBody& body_a, const RigidBody* body_b_or_null,
                                     const Contact& contact, float slop = 0.01f,
                                     float beta = 0.2f);

}  // namespace odyssey::physics
