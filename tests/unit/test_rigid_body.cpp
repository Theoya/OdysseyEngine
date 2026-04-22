#include "physics/rigid_body.h"
#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <cmath>

using namespace odyssey;
using namespace odyssey::physics;

class RigidBodyTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
};

// Free fall test: body with gravity but no forces should accelerate downward.
TEST_F(RigidBodyTest, FreeFallAccelerates) {
    RigidBody body;
    body.position = {0.0f, 10.0f, 0.0f};
    body.velocity = {0.0f, 0.0f, 0.0f};
    body.mass = 1.0f;
    body.use_gravity = true;
    body.drag = 0.0f;  // no drag

    vec3 gravity = {0.0f, -9.81f, 0.0f};
    float dt = 1.0f;

    RigidBody result = integrate(body, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, gravity, dt);

    // After 1 second of free fall:
    // v = 0 + (-9.81)*1 = -9.81 m/s
    EXPECT_NEAR(result.velocity.y, -9.81f, EPSILON);

    // Position: y' = y + v'*dt = 10 + (-9.81)*1 = 0.19 m
    // (semi-implicit uses v' for the position update)
    EXPECT_NEAR(result.position.y, 0.19f, EPSILON);
}

// Static body test: zero mass should prevent all motion.
TEST_F(RigidBodyTest, StaticBodyDoesNotMove) {
    RigidBody body;
    body.position = {0.0f, 0.0f, 0.0f};
    body.velocity = {1.0f, 1.0f, 1.0f};
    body.mass = 0.0f;  // static

    vec3 gravity = {0.0f, -9.81f, 0.0f};
    vec3 large_force = {100.0f, 100.0f, 100.0f};
    float dt = 1.0f;

    RigidBody result = integrate(body, large_force, {0.0f, 0.0f, 0.0f}, gravity, dt);

    EXPECT_EQ(result.position, vec3(0.0f));
    EXPECT_EQ(result.velocity, vec3(0.0f));
}

// Kinematic body test: kinematic bodies ignore forces.
TEST_F(RigidBodyTest, KinematicBodyIgnoresForces) {
    RigidBody body;
    body.position = {0.0f, 0.0f, 0.0f};
    body.velocity = {1.0f, 0.0f, 0.0f};
    body.mass = 1.0f;
    body.is_kinematic = true;

    vec3 gravity = {0.0f, -100.0f, 0.0f};  // large gravity
    vec3 large_force = {1000.0f, 0.0f, 0.0f};
    float dt = 0.1f;

    RigidBody result = integrate(body, large_force, {0.0f, 0.0f, 0.0f}, gravity, dt);

    // Velocity should remain unchanged.
    EXPECT_EQ(result.velocity, vec3(1.0f, 0.0f, 0.0f));

    // Position should only move by the existing velocity.
    EXPECT_NEAR(result.position.x, 0.1f, EPSILON);
    EXPECT_EQ(result.position.y, 0.0f);
}

// Drag test: linear drag should oppose motion and eventually stop a body.
TEST_F(RigidBodyTest, LinearDragReducesVelocity) {
    RigidBody body;
    body.position = {0.0f, 0.0f, 0.0f};
    body.velocity = {10.0f, 0.0f, 0.0f};
    body.mass = 1.0f;
    body.drag = 1.0f;  // strong drag
    body.use_gravity = false;

    vec3 zero_force = {0.0f, 0.0f, 0.0f};
    float dt = 0.1f;

    RigidBody result = integrate(body, zero_force, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, dt);

    // With drag coefficient 1.0 and dt 0.1:
    // a = -drag * v = -1.0 * 10 = -10
    // v' = 10 + (-10)*0.1 = 9.0
    EXPECT_NEAR(result.velocity.x, 9.0f, EPSILON);
}

// Gravity disabled test.
TEST_F(RigidBodyTest, GravityCanBeDisabled) {
    RigidBody body;
    body.position = {0.0f, 10.0f, 0.0f};
    body.velocity = {0.0f, 0.0f, 0.0f};
    body.mass = 1.0f;
    body.use_gravity = false;
    body.drag = 0.0f;

    vec3 gravity = {0.0f, -100.0f, 0.0f};
    float dt = 1.0f;

    RigidBody result = integrate(body, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, gravity, dt);

    // With gravity disabled, the body should not accelerate downward.
    EXPECT_NEAR(result.velocity.y, 0.0f, EPSILON);
    EXPECT_NEAR(result.position.y, 10.0f, EPSILON);
}

// Applied force test.
TEST_F(RigidBodyTest, AppliedForceAccelerates) {
    RigidBody body;
    body.position = {0.0f, 0.0f, 0.0f};
    body.velocity = {0.0f, 0.0f, 0.0f};
    body.mass = 2.0f;
    body.use_gravity = false;
    body.drag = 0.0f;

    vec3 force = {20.0f, 0.0f, 0.0f};  // F = 20 N
    float dt = 0.5f;

    RigidBody result = integrate(body, force, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, dt);

    // a = F / m = 20 / 2 = 10 m/s^2
    // v' = 0 + 10*0.5 = 5 m/s
    EXPECT_NEAR(result.velocity.x, 5.0f, EPSILON);

    // x' = 0 + 5*0.5 = 2.5 m
    EXPECT_NEAR(result.position.x, 2.5f, EPSILON);
}

// Angular drag test.
TEST_F(RigidBodyTest, AngularDragReducesAngularVelocity) {
    RigidBody body;
    body.angular_velocity = {10.0f, 0.0f, 0.0f};
    body.angular_drag = 0.5f;
    body.mass = 1.0f;

    float dt = 1.0f;
    RigidBody result = integrate(body, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, dt);

    // ω' = ω * (1 - angular_drag*dt) = 10 * (1 - 0.5*1) = 5 m/s
    EXPECT_NEAR(result.angular_velocity.x, 5.0f, EPSILON);
}

// Sphere inertia test.
TEST_F(RigidBodyTest, SphereInertia) {
    float mass = 2.0f;
    float radius = 0.5f;
    auto I = sphere_inertia(mass, radius);

    // For sphere: I = (2/5)*m*r^2 = (2/5)*2*0.25 = 0.2
    float expected_I = (2.0f / 5.0f) * mass * radius * radius;
    EXPECT_NEAR(I[0][0], expected_I, EPSILON);
    EXPECT_NEAR(I[1][1], expected_I, EPSILON);
    EXPECT_NEAR(I[2][2], expected_I, EPSILON);

    // Off-diagonal should be zero.
    EXPECT_NEAR(I[0][1], 0.0f, EPSILON);
    EXPECT_NEAR(I[1][2], 0.0f, EPSILON);
}

// Box inertia test.
TEST_F(RigidBodyTest, BoxInertia) {
    float mass = 3.0f;
    vec3 half_extents = {1.0f, 2.0f, 3.0f};
    auto I = box_inertia(mass, half_extents);

    // I_xx = (m/3) * (b^2 + c^2) = 1 * (4 + 9) = 13
    float Ixx = (mass / 3.0f) * (2*2 + 3*3);
    float Iyy = (mass / 3.0f) * (1*1 + 3*3);
    float Izz = (mass / 3.0f) * (1*1 + 2*2);

    EXPECT_NEAR(I[0][0], Ixx, EPSILON);
    EXPECT_NEAR(I[1][1], Iyy, EPSILON);
    EXPECT_NEAR(I[2][2], Izz, EPSILON);

    // Off-diagonal should be zero.
    EXPECT_NEAR(I[0][1], 0.0f, EPSILON);
    EXPECT_NEAR(I[1][2], 0.0f, EPSILON);
}

// Negative mass (infinite static) test.
TEST_F(RigidBodyTest, NegativeMassIsTreatedAsStatic) {
    RigidBody body;
    body.position = {5.0f, 5.0f, 5.0f};
    body.velocity = {10.0f, 10.0f, 10.0f};
    body.mass = -1.0f;  // negative = infinite / static

    RigidBody result = integrate(body, {100.0f, 100.0f, 100.0f}, {0.0f, 0.0f, 0.0f},
                                {0.0f, -100.0f, 0.0f}, 1.0f);

    EXPECT_EQ(result.position, vec3(5.0f));
    EXPECT_EQ(result.velocity, vec3(0.0f));
}
