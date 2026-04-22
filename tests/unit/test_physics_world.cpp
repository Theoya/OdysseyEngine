#include "physics/physics_world.h"
#include <gtest/gtest.h>
#include <glm/glm.hpp>

using namespace odyssey;
using namespace odyssey::physics;

class PhysicsWorldTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-3f;
};

TEST_F(PhysicsWorldTest, GravityAcceleratesBody) {
    PhysicsWorld world;
    world.gravity = {0.0f, -10.0f, 0.0f};
    world.ground_y = -100.0f;

    RigidBody body;
    body.mass = 1.0f;
    body.use_gravity = true;
    body.drag = 0.0f;

    uint32_t id = world.add_body(body);
    world.step(1.0f / 60.0f);

    const RigidBody& result = world.body(id);
    EXPECT_LT(result.velocity.y, 0.0f);
    EXPECT_GT(result.velocity.y, -0.2f);
}

TEST_F(PhysicsWorldTest, MultipleBodiesTogetherSimulate) {
    PhysicsWorld world;
    world.gravity = {0.0f, -9.81f, 0.0f};
    world.ground_y = 0.0f;

    std::vector<uint32_t> ids;
    for (int i = 0; i < 5; i++) {
        RigidBody sphere;
        sphere.position = {static_cast<float>(i), 5.0f + i, 0.0f};
        sphere.mass = 1.0f;
        sphere.use_gravity = true;
        uint32_t id = world.add_body(sphere);
        ids.push_back(id);
        SphereCollider collider;
        collider.radius = 0.5f;
        world.add_sphere_collider(id, collider);
    }

    for (int step = 0; step < 100; step++) {
        world.step(1.0f / 60.0f);
    }

    EXPECT_EQ(world.body_count(), 5);
}

TEST_F(PhysicsWorldTest, KinematicBodyUnaffectedByGravity) {
    PhysicsWorld world;
    world.gravity = {0.0f, -100.0f, 0.0f};
    world.ground_y = -1000.0f;

    RigidBody kinematic;
    kinematic.position = {0.0f, 5.0f, 0.0f};
    kinematic.velocity = {1.0f, 0.0f, 0.0f};
    kinematic.mass = 1.0f;
    kinematic.is_kinematic = true;

    uint32_t id = world.add_body(kinematic);
    SphereCollider collider;
    collider.radius = 0.5f;
    world.add_sphere_collider(id, collider);

    world.step(1.0f);

    const RigidBody& result = world.body(id);
    EXPECT_NEAR(result.position.x, 1.0f, EPSILON);
    EXPECT_NEAR(result.position.y, 5.0f, EPSILON);
    EXPECT_NEAR(result.velocity.y, 0.0f, EPSILON);
}

TEST_F(PhysicsWorldTest, StaticBodyUnmovable) {
    PhysicsWorld world;
    world.gravity = {0.0f, -9.81f, 0.0f};
    world.ground_y = 0.0f;

    RigidBody static_body;
    static_body.position = {0.0f, 5.0f, 0.0f};
    static_body.mass = 0.0f;

    uint32_t id = world.add_body(static_body);
    SphereCollider collider;
    collider.radius = 0.5f;
    world.add_sphere_collider(id, collider);

    world.step(10.0f);

    const RigidBody& result = world.body(id);
    EXPECT_EQ(result.position.y, 5.0f);
    EXPECT_EQ(result.velocity, vec3(0.0f));
}
