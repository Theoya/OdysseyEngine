#include "editor/scene_camera.h"
#include <gtest/gtest.h>

using namespace odyssey::editor;
using glm::vec3;

// Additional raycast_ground edge cases and integration tests

TEST(RaycastGroundEdgeTest, RayNearlyParallel) {
    // Ray nearly parallel to plane (very small Y component).
    vec3 origin(0.0f, 10.0f, 0.0f);
    vec3 direction(1.0f, 0.0001f, 0.0f);  // Almost horizontal

    auto hit = raycast_ground(origin, direction);

    // Ray pointing away should not intersect.
    EXPECT_FALSE(hit.has_value());
}

TEST(RaycastGroundEdgeTest, RayBeyondFloatPrecision) {
    // Ray from very far away.
    vec3 origin(0.0f, 1e6f, 0.0f);
    vec3 direction(0.0f, -1.0f, 0.0f);

    auto hit = raycast_ground(origin, direction);

    EXPECT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->y, 0.0f, 1e-3f);
}

TEST(RaycastGroundEdgeTest, RayStartingOnGround) {
    vec3 origin(5.0f, 0.0f, -3.0f);
    vec3 direction(0.0f, -1.0f, 0.0f);

    auto hit = raycast_ground(origin, direction);

    // t = 0 / -1 = 0, which is valid (ray starts on ground).
    EXPECT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->y, 0.0f, 1e-4f);
    EXPECT_NEAR(hit->x, 5.0f, 1e-4f);
    EXPECT_NEAR(hit->z, -3.0f, 1e-4f);
}

TEST(RaycastGroundEdgeTest, NormalizedVsUnnormalized) {
    vec3 origin(0.0f, 10.0f, 0.0f);
    vec3 direction_norm = glm::normalize(vec3(2.0f, -1.0f, 2.0f));
    vec3 direction_unnorm(2.0f, -1.0f, 2.0f);

    auto hit_norm = raycast_ground(origin, direction_norm);
    auto hit_unnorm = raycast_ground(origin, direction_unnorm);

    // Both should hit at the same point (raycast is linear, scales don't matter).
    EXPECT_TRUE(hit_norm.has_value());
    EXPECT_TRUE(hit_unnorm.has_value());
    EXPECT_NEAR(hit_norm->x, hit_unnorm->x, 0.01f);
    EXPECT_NEAR(hit_norm->y, hit_unnorm->y, 1e-4f);
    EXPECT_NEAR(hit_norm->z, hit_unnorm->z, 0.01f);
}
