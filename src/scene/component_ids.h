#pragma once
#include <cstdint>

namespace odyssey::scene {

// Phase 9: Stable-ordinal component IDs (never reorder, used for protocol versioning).
// Per Phase 9 netcode condition: explicit enum values enable future protocol v3
// replication to reference components by ordinal rather than insertion order.
enum class ComponentId : uint16_t {
    // Phase 1-6 legacy
    Transform = 0,
    Stats = 1,
    MeshRenderer = 2,
    Behavior = 3,
    Script = 4,
    Tags = 5,
    VoiceSource = 6,
    PrefabSource = 7,

    // Phase 9: Physics + Camera
    Rigidbody = 8,
    BoxCollider = 9,
    SphereCollider = 10,
    CapsuleCollider = 11,
    MeshCollider = 12,
    Camera = 13,

    // DO NOT reorder or reuse values above. Future protocol v3 will reference
    // components by ordinal. Insert new components after kMaxCurrentId.
};

}  // namespace odyssey::scene
