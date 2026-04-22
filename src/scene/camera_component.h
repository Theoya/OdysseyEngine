#pragma once

namespace odyssey::scene {

// Phase 9: Camera component — attached to an entity to mark it as a render target.
// This component is NEVER replicated (client-local only).
struct CameraComponent {
    // Vertical field-of-view in degrees
    float fov = 70.0f;

    // Near plane distance in meters
    float near_plane = 0.1f;

    // Far plane distance in meters
    float far_plane = 1000.0f;

    // If true, this camera is the active render camera for the scene.
    // Only one camera per scene should have is_main=true.
    bool is_main = false;

    // Phase 9 netcode condition: kReplicated = false means this component
    // is client-local and never crosses the wire.
    static constexpr bool kReplicated = false;
};

}  // namespace odyssey::scene
