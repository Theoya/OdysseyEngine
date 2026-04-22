#include "editor/scene_camera.h"
#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace odyssey::editor;
using glm::vec3;
using glm::mat4;

// ============================================================================
// SceneCamera Tests
// ============================================================================

TEST(SceneCameraTest, ViewMatrixIsLookAt) {
    // Camera at origin looking down +Z should produce a view matrix
    // consistent with glm::lookAt.
    SceneCamera cam;
    cam.position = glm::vec3(0.0f, 0.0f, 0.0f);
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;

    glm::mat4 view = cam.view_matrix();
    glm::mat4 expected = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f),
                                      glm::vec3(0.0f, 0.0f, 1.0f),
                                      glm::vec3(0.0f, 1.0f, 0.0f));

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_NEAR(view[i][j], expected[i][j], 1e-4f)
                << "Mismatch at [" << i << "][" << j << "]";
        }
    }
}

TEST(SceneCameraTest, ProjectionMatrixIsCorrect) {
    SceneCamera cam;
    float aspect = 16.0f / 9.0f;
    glm::mat4 proj = cam.projection_matrix(aspect);
    glm::mat4 expected = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 500.0f);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_NEAR(proj[i][j], expected[i][j], 1e-4f)
                << "Mismatch at [" << i << "][" << j << "]";
        }
    }
}

TEST(SceneCameraTest, ForwardVectorAtYawZeroPitchZero) {
    SceneCamera cam;
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;

    glm::vec3 fwd = cam.forward();
    // Should point along +Z (into screen).
    EXPECT_NEAR(fwd.x, 0.0f, 1e-4f);
    EXPECT_NEAR(fwd.y, 0.0f, 1e-4f);
    EXPECT_NEAR(fwd.z, 1.0f, 1e-4f);
}

TEST(SceneCameraTest, RightVectorAtYawZeroPitchZero) {
    SceneCamera cam;
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;

    glm::vec3 rgt = cam.right();
    // Should point along +X (right).
    EXPECT_NEAR(rgt.x, 1.0f, 1e-4f);
    EXPECT_NEAR(rgt.y, 0.0f, 1e-4f);
    EXPECT_NEAR(rgt.z, 0.0f, 1e-4f);
}

TEST(SceneCameraTest, UpVectorIsAlwaysY) {
    SceneCamera cam;
    cam.yaw = 1.234f;
    cam.pitch = 0.567f;

    glm::vec3 up = cam.up();
    // Should always be (0, 1, 0).
    EXPECT_NEAR(up.x, 0.0f, 1e-4f);
    EXPECT_NEAR(up.y, 1.0f, 1e-4f);
    EXPECT_NEAR(up.z, 0.0f, 1e-4f);
}

// ============================================================================
// update_scene_camera Tests
// ============================================================================

TEST(UpdateSceneCameraTest, NoInputNoMovement) {
    SceneCamera initial;
    initial.position = glm::vec3(1.0f, 2.0f, 3.0f);
    initial.yaw = 0.5f;
    initial.pitch = 0.2f;

    SceneCameraInput input{};  // All false/zero
    SceneCamera updated = update_scene_camera(initial, input, 0.016f);

    EXPECT_EQ(updated.position, initial.position);
    EXPECT_EQ(updated.yaw, initial.yaw);
    EXPECT_EQ(updated.pitch, initial.pitch);
}

TEST(UpdateSceneCameraTest, ForwardMovement) {
    SceneCamera initial;
    initial.position = glm::vec3(0.0f, 0.0f, 0.0f);
    initial.yaw = 0.0f;
    initial.pitch = 0.0f;

    SceneCameraInput input{};
    input.move_forward = true;
    float dt = 1.0f;  // 1 second
    SceneCamera updated = update_scene_camera(initial, input, dt);

    // Base speed is 10 m/s, forward direction is (0, 0, 1).
    // Expected: position = (0, 0, 0) + (0, 0, 1) * 10 = (0, 0, 10).
    EXPECT_NEAR(updated.position.x, 0.0f, 1e-3f);
    EXPECT_NEAR(updated.position.y, 0.0f, 1e-3f);
    EXPECT_NEAR(updated.position.z, 10.0f, 1e-3f);
}

TEST(UpdateSceneCameraTest, RightMouseRotation) {
    SceneCamera initial;
    initial.yaw = 0.0f;
    initial.pitch = 0.0f;

    SceneCameraInput input{};
    input.right_button_held = true;
    input.mouse_dx = 100.0f;  // pixels
    input.mouse_dy = 50.0f;   // pixels

    SceneCamera updated = update_scene_camera(initial, input, 0.016f);

    // Sensitivity is 0.005 rad/px.
    // Yaw -= 100 * 0.005 = -0.5 rad.
    // Pitch += 50 * 0.005 = 0.25 rad.
    EXPECT_NEAR(updated.yaw, -0.5f, 1e-4f);
    EXPECT_NEAR(updated.pitch, 0.25f, 1e-4f);
}

TEST(UpdateSceneCameraTest, PitchClamping) {
    SceneCamera initial;
    initial.pitch = 0.0f;

    SceneCameraInput input{};
    input.right_button_held = true;
    input.mouse_dy = 10000.0f;  // Huge value to exceed clamp limit

    SceneCamera updated = update_scene_camera(initial, input, 0.016f);

    // Pitch should be clamped to roughly ±88.2° (glm::pi * 0.49).
    float max_pitch = glm::pi<float>() * 0.49f;
    EXPECT_LE(updated.pitch, max_pitch + 1e-3f);
}

TEST(UpdateSceneCameraTest, ShiftMultipliesSpeed) {
    SceneCamera initial;
    initial.position = glm::vec3(0.0f, 0.0f, 0.0f);
    initial.yaw = 0.0f;
    initial.pitch = 0.0f;

    SceneCameraInput input{};
    input.move_forward = true;
    input.shift_held = true;
    float dt = 1.0f;

    SceneCamera updated = update_scene_camera(initial, input, dt);

    // With shift: speed = 10 * 3 = 30 m/s.
    // Forward direction is (0, 0, 1), so z should be 30.
    EXPECT_NEAR(updated.position.z, 30.0f, 1e-3f);
}

TEST(UpdateSceneCameraTest, WorldYAxisMovement) {
    SceneCamera initial;
    initial.position = glm::vec3(0.0f, 0.0f, 0.0f);
    initial.yaw = 1.234f;  // Arbitrary non-zero
    initial.pitch = 0.567f;

    SceneCameraInput input{};
    input.move_up = true;
    float dt = 1.0f;

    SceneCamera updated = update_scene_camera(initial, input, dt);

    // Move up (world Y) regardless of camera orientation.
    // speed = 10 m/s * 1.0s = 10.
    EXPECT_NEAR(updated.position.x, 0.0f, 1e-3f);
    EXPECT_NEAR(updated.position.y, 10.0f, 1e-3f);
    EXPECT_NEAR(updated.position.z, 0.0f, 1e-3f);
}

TEST(UpdateSceneCameraTest, ScrollDolly) {
    SceneCamera initial;
    initial.position = glm::vec3(0.0f, 0.0f, 0.0f);
    initial.yaw = 0.0f;
    initial.pitch = 0.0f;

    SceneCameraInput input{};
    input.scroll_delta = 2.0f;  // 2 notches
    float dt = 0.016f;          // Doesn't matter for scroll

    SceneCamera updated = update_scene_camera(initial, input, dt);

    // Dolly speed = 5 m/notch.
    // Forward direction is (0, 0, 1).
    // position += (0, 0, 1) * (2.0 * 5.0) = (0, 0, 10).
    EXPECT_NEAR(updated.position.z, 10.0f, 1e-3f);
}

// ============================================================================
// compute_frame_target Tests
// ============================================================================

TEST(ComputeFrameTargetTest, EntityAtOrigin) {
    vec3 entity_pos(0.0f, 0.0f, 0.0f);
    float radius = 1.0f;

    FrameTarget target = compute_frame_target(entity_pos, radius);

    // Camera should be 3 * 1 = 3 units behind entity.
    // Direction with yaw=0, pitch=-15° means forward = (0, -sin(15°), cos(15°)).
    // Position = entity_pos - fwd * 3 = (0, 0, 0) - (0, -0.259, 0.966) * 3
    //          ≈ (0, 0.776, -2.887).
    EXPECT_NEAR(target.yaw, 0.0f, 1e-4f);
    EXPECT_NEAR(target.pitch, glm::radians(-15.0f), 1e-4f);
    EXPECT_NEAR(target.position.x, 0.0f, 0.02f);
    EXPECT_NEAR(target.position.y, 0.776f, 0.02f);
    EXPECT_NEAR(target.position.z, -2.887f, 0.02f);
}

TEST(ComputeFrameTargetTest, EntityAboveOrigin) {
    vec3 entity_pos(5.0f, 10.0f, -3.0f);
    float radius = 2.0f;

    FrameTarget target = compute_frame_target(entity_pos, radius);

    // 6 units away (3 * 2).
    // Position ≈ (5.0f, 10 + 1.552, -3 - 5.774) = (5.0, 11.552, -8.774).
    EXPECT_NEAR(target.position.x, 5.0f, 1e-2f);
    EXPECT_NEAR(target.position.y, 10.0f + 1.552f, 0.02f);
    EXPECT_NEAR(target.position.z, -3.0f - 5.774f, 0.03f);
}

// ============================================================================
// raycast_ground Tests
// ============================================================================

TEST(RaycastGroundTest, RayFromAbovePointingDown) {
    vec3 origin(0.0f, 5.0f, 0.0f);
    vec3 direction(0.0f, -1.0f, 0.0f);  // Straight down

    auto hit = raycast_ground(origin, direction);

    EXPECT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->x, 0.0f, 1e-4f);
    EXPECT_NEAR(hit->y, 0.0f, 1e-4f);
    EXPECT_NEAR(hit->z, 0.0f, 1e-4f);
}

TEST(RaycastGroundTest, RayParallelToGround) {
    vec3 origin(0.0f, 5.0f, 0.0f);
    vec3 direction(1.0f, 0.0f, 0.0f);  // Horizontal

    auto hit = raycast_ground(origin, direction);

    EXPECT_FALSE(hit.has_value());
}

TEST(RaycastGroundTest, RayPointingUpFromBelowGround) {
    vec3 origin(0.0f, -5.0f, 0.0f);
    vec3 direction(0.0f, 1.0f, 0.0f);  // Pointing up

    auto hit = raycast_ground(origin, direction);

    EXPECT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->y, 0.0f, 1e-4f);
}

TEST(RaycastGroundTest, RayAtAngle) {
    vec3 origin(0.0f, 10.0f, 0.0f);
    vec3 direction(1.0f, -1.0f, 1.0f);
    direction = glm::normalize(direction);

    auto hit = raycast_ground(origin, direction);

    EXPECT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->y, 0.0f, 1e-4f);
    // t = -10.0 / (-direction.y) = 10.0 / direction.y
    // Since direction.y ≈ -0.577, t ≈ 17.32.
    // x = 0 + t * direction.x ≈ 17.32 * 0.577 ≈ 10.0
    EXPECT_NEAR(hit->x, 10.0f, 0.1f);
    EXPECT_NEAR(hit->z, 10.0f, 0.1f);
}

TEST(RaycastGroundTest, RayPointingAwayFromGround) {
    vec3 origin(0.0f, 5.0f, 0.0f);
    vec3 direction(0.0f, 1.0f, 0.0f);  // Pointing up

    auto hit = raycast_ground(origin, direction);

    EXPECT_FALSE(hit.has_value());
}
