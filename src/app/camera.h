#pragma once

#include "core/types.h"

namespace odyssey {

/// FPS-style camera. Pure math — no GLFW dependency.
class Camera {
public:
    Camera();

    /// Move and look based on input state.
    /// Mouse deltas are raw pixel offsets; sensitivity is applied internally.
    void update(float delta_time,
                bool forward, bool backward, bool left, bool right,
                bool up, bool down,
                float mouse_dx, float mouse_dy);

    /// Current view matrix (glm::lookAt).
    mat4 view_matrix() const;

    /// Perspective projection matrix.
    mat4 projection_matrix(float aspect, float fov = 45.0f,
                           float near_plane = 0.1f, float far_plane = 500.0f) const;

    /// Combined view-projection matrix.
    mat4 vp_matrix(float aspect) const;

    /// Accessors
    vec3 position() const { return position_; }
    void set_position(vec3 pos) { position_ = pos; }
    vec3 front() const { return front_; }
    vec3 right() const { return right_; }

    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }

private:
    /// Recompute the front/right/up basis vectors from yaw and pitch.
    void update_vectors();

    vec3 position_;
    float yaw_;    // radians
    float pitch_;  // radians

    vec3 front_;
    vec3 right_;
    vec3 up_;

    static constexpr float MOVE_SPEED = 10.0f;
    static constexpr float MOUSE_SENSITIVITY = 0.002f;
};

} // namespace odyssey
