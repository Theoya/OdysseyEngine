#include "physics/colliders.h"
#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <cmath>

using namespace odyssey;
using namespace odyssey::physics;

class CollidersTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
};

TEST_F(CollidersTest, SphereSphereJustTouching) {
    SphereCollider a;
    a.radius = 1.0f;
    SphereCollider b;
    b.radius = 1.0f;
    vec3 pa = {0.0f, 0.0f, 0.0f};
    vec3 pb = {2.0f, 0.0f, 0.0f};
    auto contact = collide_sphere_sphere(a, pa, b, pb);
    EXPECT_FALSE(contact.has_value());
}

TEST_F(CollidersTest, SphereSpherePenetrating) {
    SphereCollider a, b;
    a.radius = b.radius = 1.0f;
    vec3 pa = {0.0f, 0.0f, 0.0f};
    vec3 pb = {1.0f, 0.0f, 0.0f};
    auto contact = collide_sphere_sphere(a, pa, b, pb);
    ASSERT_TRUE(contact.has_value());
    EXPECT_NEAR(contact->penetration, 1.0f, EPSILON);
}

TEST_F(CollidersTest, SphereSphereSeparated) {
    SphereCollider a, b;
    a.radius = b.radius = 1.0f;
    vec3 pa = {0.0f, 0.0f, 0.0f};
    vec3 pb = {10.0f, 0.0f, 0.0f};
    auto contact = collide_sphere_sphere(a, pa, b, pb);
    EXPECT_FALSE(contact.has_value());
}

TEST_F(CollidersTest, SphereSphereCoincenters) {
    SphereCollider a, b;
    a.radius = b.radius = 1.0f;
    vec3 pos = {0.0f, 0.0f, 0.0f};
    auto contact = collide_sphere_sphere(a, pos, b, pos);
    ASSERT_TRUE(contact.has_value());
    EXPECT_NEAR(contact->penetration, 2.0f, EPSILON);
}

TEST_F(CollidersTest, SphereGroundResting) {
    SphereCollider sphere;
    sphere.radius = 1.0f;
    vec3 pos = {0.0f, 1.0f, 0.0f};  // center 1 unit above ground, so bottom touches at y=0
    auto contact = collide_sphere_ground(sphere, pos, 0.0f);
    ASSERT_TRUE(contact.has_value());
    EXPECT_NEAR(contact->penetration, 0.0f, EPSILON);  // Just touching, no penetration
}

TEST_F(CollidersTest, SphereGroundPenetrating) {
    SphereCollider sphere;
    sphere.radius = 1.0f;
    vec3 pos = {0.0f, 0.5f, 0.0f};
    auto contact = collide_sphere_ground(sphere, pos, 0.0f);
    ASSERT_TRUE(contact.has_value());
    EXPECT_NEAR(contact->penetration, 0.5f, EPSILON);
}

TEST_F(CollidersTest, SphereGroundAbove) {
    SphereCollider sphere;
    sphere.radius = 1.0f;
    vec3 pos = {0.0f, 5.0f, 0.0f};
    auto contact = collide_sphere_ground(sphere, pos, 0.0f);
    EXPECT_FALSE(contact.has_value());
}

TEST_F(CollidersTest, SphereSphereInBoxPenetrating) {
    SphereCollider sphere;
    sphere.radius = 0.5f;
    BoxCollider box;
    box.half_extents = {1.0f, 1.0f, 1.0f};
    vec3 ps = {0.0f, 0.0f, 0.0f};
    vec3 pb = {0.0f, 0.0f, 0.0f};
    auto contact = collide_sphere_box(sphere, ps, box, pb);
    ASSERT_TRUE(contact.has_value());
    EXPECT_NEAR(contact->penetration, 0.5f, EPSILON);
}

TEST_F(CollidersTest, SphereBoxTouchingFace) {
    SphereCollider sphere;
    sphere.radius = 1.0f;
    BoxCollider box;
    box.half_extents = {1.0f, 1.0f, 1.0f};
    vec3 ps = {2.0f, 0.0f, 0.0f};
    vec3 pb = {0.0f, 0.0f, 0.0f};
    auto contact = collide_sphere_box(sphere, ps, box, pb);
    ASSERT_TRUE(contact.has_value());
    EXPECT_NEAR(contact->penetration, 0.0f, EPSILON);
}

TEST_F(CollidersTest, SphereBoxSeparated) {
    SphereCollider sphere;
    sphere.radius = 0.5f;
    BoxCollider box;
    box.half_extents = {1.0f, 1.0f, 1.0f};
    vec3 ps = {5.0f, 0.0f, 0.0f};
    vec3 pb = {0.0f, 0.0f, 0.0f};
    auto contact = collide_sphere_box(sphere, ps, box, pb);
    EXPECT_FALSE(contact.has_value());
}

TEST_F(CollidersTest, CapsuleGroundResting) {
    CapsuleCollider capsule;
    capsule.radius = 0.5f;
    capsule.height = 2.0f;
    vec3 pos = {0.0f, 1.0f, 0.0f};
    auto contact = collide_capsule_ground(capsule, pos, 0.0f);
    ASSERT_TRUE(contact.has_value());
    EXPECT_NEAR(contact->penetration, 0.5f, EPSILON);
}

TEST_F(CollidersTest, CapsuleGroundAbove) {
    CapsuleCollider capsule;
    capsule.radius = 0.5f;
    capsule.height = 2.0f;
    vec3 pos = {0.0f, 5.0f, 0.0f};
    auto contact = collide_capsule_ground(capsule, pos, 0.0f);
    EXPECT_FALSE(contact.has_value());
}

TEST_F(CollidersTest, CapsuleGroundPenetrating) {
    CapsuleCollider capsule;
    capsule.radius = 0.5f;
    capsule.height = 2.0f;  // bottom at -0.8, top at 1.2
    vec3 pos = {0.0f, 0.2f, 0.0f};  // center at 0.2, closest axis point is 0, surface at -0.5
    auto contact = collide_capsule_ground(capsule, pos, 0.0f);
    ASSERT_TRUE(contact.has_value());
    // Expected penetration: ground_y (0) - surface_y (-0.5) = 0.5
    EXPECT_NEAR(contact->penetration, 0.5f, EPSILON);
}

TEST_F(CollidersTest, SphereBoxNormalDirection) {
    SphereCollider sphere;
    sphere.radius = 1.0f;
    BoxCollider box;
    box.half_extents = {1.0f, 1.0f, 1.0f};
    vec3 ps = {2.0f, 0.0f, 0.0f};
    vec3 pb = {0.0f, 0.0f, 0.0f};
    auto contact = collide_sphere_box(sphere, ps, box, pb);
    ASSERT_TRUE(contact.has_value());
    EXPECT_GT(contact->normal.x, 0.5f);
}
