#include "physics/contact_solver.h"
#include <gtest/gtest.h>
#include <glm/glm.hpp>

using namespace odyssey;
using namespace odyssey::physics;

class ContactSolverTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-3f;
};

// Test that position correction is applied when penetration > slop
TEST_F(ContactSolverTest, BaumgartePositionCorrection) {
    RigidBody a, b;
    a.mass = b.mass = 1.0f;
    Contact contact;
    contact.normal = {1.0f, 0.0f, 0.0f};
    contact.penetration = 0.1f;
    PositionCorrection corr = correct_positions(a, &b, contact, 0.01f, 0.2f);
    EXPECT_LT(corr.delta_a.x, 0.0f);
    EXPECT_GT(corr.delta_b.x, 0.0f);
}

// Test that position correction doesn't apply when penetration <= slop
TEST_F(ContactSolverTest, SlopPreventsSmallCorrections) {
    RigidBody body;
    body.mass = 1.0f;
    Contact contact;
    contact.normal = {0.0f, 1.0f, 0.0f};
    contact.penetration = 0.005f;
    PositionCorrection corr = correct_positions(body, nullptr, contact, 0.01f, 0.2f);
    EXPECT_EQ(corr.delta_a, vec3(0.0f));
}

// Test that bodies separating don't get impulse
TEST_F(ContactSolverTest, SeparatingBodiesNoImpulse) {
    RigidBody a, b;
    a.velocity = {1.0f, 0.0f, 0.0f};
    a.mass = 1.0f;
    b.velocity = {2.0f, 0.0f, 0.0f};
    b.mass = 1.0f;
    Contact contact;
    contact.normal = {1.0f, 0.0f, 0.0f};
    SolverInput input;
    input.body_a = &a;
    input.body_b = &b;
    input.contact = contact;
    input.restitution = 0.0f;
    input.friction = 0.0f;
    SolverOutput output = solve_contact(input);
    EXPECT_EQ(output.impulse_a, vec3(0.0f));
    EXPECT_EQ(output.impulse_b, vec3(0.0f));
}

// Test that static/kinematic bodies get zero impulse when body_b is null
TEST_F(ContactSolverTest, NullBodyBHandling) {
    RigidBody a;
    a.velocity = {1.0f, 0.0f, 0.0f};
    a.mass = 1.0f;
    Contact contact;
    contact.normal = {1.0f, 0.0f, 0.0f};
    SolverInput input;
    input.body_a = &a;
    input.body_b = nullptr;
    input.contact = contact;
    input.restitution = 0.0f;
    input.friction = 0.0f;
    SolverOutput output = solve_contact(input);
    EXPECT_EQ(output.impulse_b, vec3(0.0f));
}
