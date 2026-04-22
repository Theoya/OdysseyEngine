#pragma once

// ---------------------------------------------------------------------------
// scene_camera.h
// Batch D: Pure free-fly scene camera for the Unity-class editor viewport.
//
// This is an alternative to the auto-orbit camera, with:
//   - Right-mouse held: pitch/yaw from mouse delta (sensitivity 0.005 rad/px).
//   - While right-held: WASD translates along camera basis (W=forward, S=back,
//     A=left, D=right). Q/E translates down/up (world Y).
//   - Shift held: 3x speed multiplier.
//   - Scroll wheel: dolly (translate along forward).
//   - Base speed: 10 m/s.
//
// Pure function: all math, no I/O.
// ---------------------------------------------------------------------------

#include "core/types.h"
#include <optional>
#include <cmath>

namespace odyssey::editor {

/// Immutable free-fly camera state.
struct SceneCamera {
    vec3 position = {0.0f, 5.0f, 10.0f};
    float yaw = 0.0f;    // radians
    float pitch = 0.0f;  // radians
    float fov = 45.0f;
    float near_plane = 0.1f;
    float far_plane = 500.0f;

    /// View matrix (glm::lookAt).
    mat4 view_matrix() const;

    /// Perspective projection matrix.
    mat4 projection_matrix(float aspect) const;

    /// Combined view-projection matrix.
    mat4 vp_matrix(float aspect) const;

    /// Forward/right/up basis vectors derived from yaw/pitch.
    vec3 forward() const;
    vec3 right() const;
    vec3 up() const;
};

/// Input state for camera update.
struct SceneCameraInput {
    // Keyboard movement (world axes)
    bool move_forward = false;
    bool move_backward = false;
    bool move_left = false;
    bool move_right = false;
    bool move_up = false;
    bool move_down = false;

    // Mouse state (only read when right_button_held)
    bool right_button_held = false;
    float mouse_dx = 0.0f;  // pixels
    float mouse_dy = 0.0f;  // pixels

    // Modifiers
    bool shift_held = false;

    // Scroll input (positive = zoom in)
    float scroll_delta = 0.0f;
};

/// Pure function: update camera state given input and time delta.
/// Returns new camera state.
SceneCamera update_scene_camera(const SceneCamera& current,
                                const SceneCameraInput& input,
                                float delta_time);

/// Pure helper: given entity position and radius, compute framing camera pose.
/// Positions camera 3x radius behind entity, looking at it with pitch=-15°.
struct FrameTarget {
    vec3 position;
    float yaw;
    float pitch;
};

FrameTarget compute_frame_target(vec3 entity_pos, float entity_radius = 1.0f);

/// Pure helper: raycast against Y=0 plane.
/// Returns intersection point, or std::nullopt if ray is parallel/backwards.
std::optional<vec3> raycast_ground(vec3 ray_origin, vec3 ray_direction);

} // namespace odyssey::editor
