#include "app/camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace odyssey {

// Pitch clamp in radians: +/- 89 degrees
static constexpr float MAX_PITCH = glm::radians(89.0f);

Camera::Camera()
    : position_(0.0f, 2.0f, -5.0f)
    , yaw_(glm::half_pi<float>())        // PI/2  (looking towards +Z, into the arena)
    , pitch_(0.0f)
{
    update_vectors();
}

void Camera::update(float delta_time,
                    bool forward, bool backward, bool left, bool right,
                    bool up, bool down,
                    float mouse_dx, float mouse_dy) {
    // --- Mouse look ---
    yaw_   += mouse_dx * MOUSE_SENSITIVITY;
    pitch_ -= mouse_dy * MOUSE_SENSITIVITY;   // inverted Y — moving mouse up looks up
    pitch_  = std::clamp(pitch_, -MAX_PITCH, MAX_PITCH);

    update_vectors();

    // --- Keyboard movement ---
    float velocity = MOVE_SPEED * delta_time;

    // Movement on the horizontal plane: use front projected onto XZ.
    vec3 flat_front = glm::normalize(vec3(front_.x, 0.0f, front_.z));
    vec3 flat_right = right_;   // already horizontal

    if (forward)  position_ += flat_front * velocity;
    if (backward) position_ -= flat_front * velocity;
    if (right)    position_ += flat_right * velocity;
    if (left)     position_ -= flat_right * velocity;
    if (up)       position_ += vec3(0.0f, 1.0f, 0.0f) * velocity;
    if (down)     position_ -= vec3(0.0f, 1.0f, 0.0f) * velocity;
}

mat4 Camera::view_matrix() const {
    return glm::lookAt(position_, position_ + front_, up_);
}

mat4 Camera::projection_matrix(float aspect, float fov,
                                float near_plane, float far_plane) const {
    return glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
}

mat4 Camera::vp_matrix(float aspect) const {
    return projection_matrix(aspect) * view_matrix();
}

void Camera::update_vectors() {
    // Spherical to Cartesian
    front_.x = std::cos(yaw_) * std::cos(pitch_);
    front_.y = std::sin(pitch_);
    front_.z = std::sin(yaw_) * std::cos(pitch_);
    front_   = glm::normalize(front_);

    // Right and up from world-up cross products
    right_ = glm::normalize(glm::cross(front_, vec3(0.0f, 1.0f, 0.0f)));
    up_    = glm::normalize(glm::cross(right_, front_));
}

} // namespace odyssey
