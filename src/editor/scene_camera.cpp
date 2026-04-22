#include "editor/scene_camera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace odyssey::editor {

// ============================================================================
// SceneCamera methods
// ============================================================================

mat4 SceneCamera::view_matrix() const {
    return glm::lookAt(position, position + forward(), up());
}

mat4 SceneCamera::projection_matrix(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, near_plane, far_plane);
}

mat4 SceneCamera::vp_matrix(float aspect) const {
    return projection_matrix(aspect) * view_matrix();
}

vec3 SceneCamera::forward() const {
    // Standard spherical: cos(pitch) * cos(yaw) for X, sin(pitch) for Y,
    // cos(pitch) * sin(yaw) for Z. We use yaw measured from +Z (into screen).
    return glm::normalize(vec3(
        std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::cos(yaw) * std::cos(pitch)
    ));
}

vec3 SceneCamera::right() const {
    // Right vector is up × forward (to get rightward direction).
    // For yaw=0, pitch=0: forward = (0, 0, 1), up = (0, 1, 0)
    // right = cross((0,1,0), (0,0,1)) = (1*1 - 0*0, 0*0 - 0*1, 0*0 - 1*0) = (1, 0, 0)
    return glm::normalize(glm::cross(up(), forward()));
}

vec3 SceneCamera::up() const {
    // World up (Y-axis).
    return vec3(0.0f, 1.0f, 0.0f);
}

// ============================================================================
// update_scene_camera: pure update function
// ============================================================================

SceneCamera update_scene_camera(const SceneCamera& current,
                                const SceneCameraInput& input,
                                float delta_time) {
    SceneCamera next = current;

    // Rotation from right-mouse drag (sensitivity = 0.005 rad/px).
    constexpr float MOUSE_SENSITIVITY = 0.005f;
    if (input.right_button_held) {
        next.yaw -= input.mouse_dx * MOUSE_SENSITIVITY;
        next.pitch += input.mouse_dy * MOUSE_SENSITIVITY;

        // Clamp pitch to ±89° (avoid gimbal lock at the poles).
        constexpr float MAX_PITCH = glm::pi<float>() * 0.49f;  // ~88.2°
        next.pitch = std::clamp(next.pitch, -MAX_PITCH, MAX_PITCH);
    }

    // Movement speed (base 10 m/s, 3x if shift held).
    float speed = 10.0f;
    if (input.shift_held) {
        speed *= 3.0f;
    }
    float distance = speed * delta_time;

    // Compute basis vectors for the next state's orientation
    // (so WASD movement uses the new yaw/pitch orientation).
    vec3 fwd = next.forward();
    vec3 rgt = next.right();
    vec3 up_world = vec3(0.0f, 1.0f, 0.0f);

    // WASD movement along camera basis.
    if (input.move_forward) next.position += fwd * distance;
    if (input.move_backward) next.position -= fwd * distance;
    if (input.move_left) next.position -= rgt * distance;
    if (input.move_right) next.position += rgt * distance;

    // Q/E movement along world Y.
    if (input.move_up) next.position += up_world * distance;
    if (input.move_down) next.position -= up_world * distance;

    // Scroll dolly (along forward direction).
    if (input.scroll_delta != 0.0f) {
        // Treat scroll as "distance to move forward". Typical scroll_delta is
        // ±1 per notch, so we scale by a dolly speed (e.g., 5 m/notch).
        float dolly_speed = 5.0f;
        next.position += fwd * (input.scroll_delta * dolly_speed);
    }

    return next;
}

// ============================================================================
// compute_frame_target: frame selection
// ============================================================================

FrameTarget compute_frame_target(vec3 entity_pos, float entity_radius) {
    // Position camera 3 * radius behind the entity, looking at it.
    // Direction from entity to camera: default is backwards along -Z with
    // a slight upward tilt.
    //
    // We use a unit direction (0, 0, -1) (behind the entity) rotated by
    // pitch=-15° to tilt up. Then scale by 3*radius.
    //
    // Direction (0, 0, -1) in spherical coords has yaw=0 (pointing at -Z),
    // so we keep yaw=0 and use pitch=-15°.
    //
    // Camera position = entity_pos - forward_direction * (3 * radius)
    // where forward_direction is computed from yaw=0, pitch=-15°.

    FrameTarget target;
    target.yaw = 0.0f;
    target.pitch = glm::radians(-15.0f);

    // Compute forward vector with yaw=0, pitch=-15°.
    float cos_pitch = std::cos(target.pitch);
    vec3 fwd = vec3(
        0.0f,                    // sin(yaw=0) * cos(pitch)
        std::sin(target.pitch),  // sin(-15°) ≈ -0.259
        cos_pitch                // cos(yaw=0) * cos(pitch)
    );

    target.position = entity_pos - fwd * (3.0f * entity_radius);

    return target;
}

// ============================================================================
// raycast_ground: intersection with Y=0 plane
// ============================================================================

std::optional<vec3> raycast_ground(vec3 ray_origin, vec3 ray_direction) {
    // Plane equation: Y = 0, normal = (0, 1, 0).
    // Ray: P(t) = ray_origin + t * ray_direction.
    // Intersection: (ray_origin + t * ray_direction).y = 0
    // => ray_origin.y + t * ray_direction.y = 0
    // => t = -ray_origin.y / ray_direction.y

    if (std::abs(ray_direction.y) < 1e-6f) {
        // Ray is parallel to the plane (or nearly so).
        return std::nullopt;
    }

    float t = -ray_origin.y / ray_direction.y;

    // Only accept intersections in the forward direction of the ray.
    if (t < 0.0f) {
        return std::nullopt;
    }

    return ray_origin + t * ray_direction;
}

} // namespace odyssey::editor
