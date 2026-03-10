#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>

namespace odyssey {

// GLM type aliases
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using quat = glm::quat;
using mat4 = glm::mat4;
using ivec2 = glm::ivec2;
using ivec3 = glm::ivec3;
using uvec3 = glm::uvec3;

// Entity identification
using EntityID = uint32_t;
using ArchetypeID = uint32_t;
constexpr EntityID INVALID_ENTITY = UINT32_MAX;
constexpr ArchetypeID INVALID_ARCHETYPE = UINT32_MAX;

// Transform component (SoA-friendly)
struct Transform {
    vec3 position{0.0f};
    quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // identity
    vec3 scale{1.0f};
};

// Entity stats (maps to GPU SSBO)
struct EntityStats {
    float health = 100.0f;
    float max_health = 100.0f;
    float ammo = 0.0f;
    float stamina = 100.0f;
    float speed = 5.0f;
    float padding[3] = {}; // align to 32 bytes
};

// Behavior output (per-entity, written by compute shader)
struct BehaviorOutput {
    vec4 move_vector;    // xyz = direction, w = weight
    vec4 attack_target;  // xyz = target pos, w = weight
    uint32_t animation_id;
    float animation_blend;
    uint32_t sound_event;
    float sound_priority;
    float comms_signal;
    float comms_urgency;
    float padding[2];    // align to 64 bytes
};

// World state (uniform, shared across all archetypes)
struct WorldState {
    float time;
    float delta_time;
    vec4 player_position; // w unused
    uint32_t frame_number;
    uint32_t total_entities;
    float padding[2];
};

// Per-agent persistent state
struct AgentPersistState {
    uint32_t current_state;
    float state_timer;
    float cooldown_0;
    float cooldown_1;
    vec4 memory_0; // general purpose
    vec4 memory_1;
    uint32_t last_decision;
    float padding[3];
};

// Workgroup size constant
constexpr uint32_t NADIR_WORKGROUP_SIZE = 256;

} // namespace odyssey
