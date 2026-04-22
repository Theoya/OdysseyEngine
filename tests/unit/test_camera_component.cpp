#include <gtest/gtest.h>
#include "scene/camera_component.h"

using namespace odyssey::scene;

// Test: CameraComponent defaults and round-trip properties
TEST(CameraComponentTest, DefaultsAndReplication) {
    CameraComponent cam;

    // Check defaults
    EXPECT_EQ(cam.fov, 70.0f);
    EXPECT_EQ(cam.near_plane, 0.1f);
    EXPECT_EQ(cam.far_plane, 1000.0f);
    EXPECT_FALSE(cam.is_main);

    // Check that kReplicated is false (client-local only)
    EXPECT_FALSE(CameraComponent::kReplicated);
}

// Test: CameraComponent custom values
TEST(CameraComponentTest, CustomValues) {
    CameraComponent cam;
    cam.fov = 45.0f;
    cam.near_plane = 0.01f;
    cam.far_plane = 5000.0f;
    cam.is_main = true;

    EXPECT_EQ(cam.fov, 45.0f);
    EXPECT_EQ(cam.near_plane, 0.01f);
    EXPECT_EQ(cam.far_plane, 5000.0f);
    EXPECT_TRUE(cam.is_main);
}
