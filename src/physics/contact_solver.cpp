#include "physics/contact_solver.h"
#include <glm/glm.hpp>

namespace odyssey::physics {

SolverOutput solve_contact(const SolverInput& in) {
    SolverOutput out = {{0, 0, 0}, {0, 0, 0}};

    if (!in.body_a) {
        return out;
    }

    const RigidBody& a = *in.body_a;
    float mass_a_inv = (a.mass > 0.0f) ? (1.0f / a.mass) : 0.0f;
    float mass_b_inv = 0.0f;

    if (in.body_b) {
        const RigidBody& b = *in.body_b;
        mass_b_inv = (b.mass > 0.0f) ? (1.0f / b.mass) : 0.0f;
    }

    float mass_sum_inv = mass_a_inv + mass_b_inv;
    if (mass_sum_inv < 1e-6f) {
        return out;  // Both bodies are immovable.
    }

    // Compute relative velocity at contact point.
    vec3 v_a = a.velocity;
    vec3 v_b = in.body_b ? in.body_b->velocity : vec3(0.0f);
    vec3 v_rel = v_b - v_a;

    // Normal impulse magnitude.
    float v_rel_normal = glm::dot(v_rel, in.contact.normal);

    // If bodies are separating (v_rel_normal > 0), no impulse needed.
    if (v_rel_normal > 0.0f) {
        return out;
    }

    // Impulse magnitude: j_n = -(1 + e) * v_rel_n / (mass_a_inv + mass_b_inv)
    float j_n = -(1.0f + in.restitution) * v_rel_normal / mass_sum_inv;

    // Apply normal impulse.
    out.impulse_a = j_n * in.contact.normal;
    if (in.body_b) {
        out.impulse_b = -out.impulse_a;
    }

    // Tangential friction.
    // Find a tangent direction perpendicular to the normal.
    vec3 tangent;
    if (glm::abs(in.contact.normal.x) > 0.9f) {
        tangent = glm::normalize(glm::cross(in.contact.normal, vec3(0.0f, 1.0f, 0.0f)));
    } else {
        tangent = glm::normalize(glm::cross(in.contact.normal, vec3(1.0f, 0.0f, 0.0f)));
    }

    // Relative velocity along the tangent.
    float v_rel_tangent = glm::dot(v_rel, tangent);

    // Tangent impulse magnitude, limited by Coulomb cone.
    float j_t_max = in.friction * j_n;
    float j_t = -v_rel_tangent / mass_sum_inv;
    j_t = glm::clamp(j_t, -j_t_max, j_t_max);

    // Apply tangent impulse.
    out.impulse_a += j_t * tangent;
    if (in.body_b) {
        out.impulse_b -= j_t * tangent;
    }

    return out;
}

PositionCorrection correct_positions(const RigidBody& body_a, const RigidBody* body_b_or_null,
                                     const Contact& contact, float slop, float beta) {
    PositionCorrection correction = {{0, 0, 0}, {0, 0, 0}};

    float mass_a_inv = (body_a.mass > 0.0f) ? (1.0f / body_a.mass) : 0.0f;
    float mass_b_inv = 0.0f;

    if (body_b_or_null) {
        const RigidBody& b = *body_b_or_null;
        mass_b_inv = (b.mass > 0.0f) ? (1.0f / b.mass) : 0.0f;
    }

    float mass_sum_inv = mass_a_inv + mass_b_inv;
    if (mass_sum_inv < 1e-6f) {
        return correction;  // Both immovable.
    }

    // Only correct if penetration exceeds slop.
    float penetration_depth = glm::max(0.0f, contact.penetration - slop);

    // Baumgarte correction: push bodies apart proportional to penetration.
    float correction_magnitude = beta * penetration_depth / mass_sum_inv;

    correction.delta_a = -correction_magnitude * mass_a_inv * contact.normal;
    if (body_b_or_null) {
        correction.delta_b = correction_magnitude * mass_b_inv * contact.normal;
    }

    return correction;
}

}  // namespace odyssey::physics
