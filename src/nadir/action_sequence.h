#pragma once

#include "core/result.h"
#include "core/types.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace odyssey::nadir {

// ---------------------------------------------------------------------------
// Action types that can appear in a sequence step
// ---------------------------------------------------------------------------
enum class ActionType : uint32_t {
    MOVE_TO,      // Move entity to a world-space position
    WAIT,         // Pause for a fixed duration
    PLAY_ANIM,    // Play animation by ID for a duration
    PLAY_SOUND,   // Fire a sound/voice-line event by name
    SET_STATE,    // Write a value into AgentPersistState
    LOOK_AT,      // Face a target position or named entity
    SPAWN,        // Spawn a prefab at an offset
    DESTROY_SELF, // Remove this entity from the world
    EMIT_SIGNAL,  // Broadcast a comms_signal value in the output buffer
    BRANCH,       // Conditional jump: read persist state key, jump if non-zero
};

// ---------------------------------------------------------------------------
// A single step inside a sequence
// All fields are present; only the ones relevant to the ActionType are used.
// ---------------------------------------------------------------------------
struct ActionStep {
    ActionType type = ActionType::WAIT;

    // MOVE_TO, LOOK_AT
    vec3     target_position{0.0f};

    // WAIT, PLAY_ANIM
    float    duration     = 0.0f;

    // PLAY_ANIM
    uint32_t anim_id      = 0;

    // PLAY_SOUND (event ID, if a numeric ID was given in the XML)
    uint32_t sound_id     = 0;

    // SET_STATE: which slot in AgentPersistState to write
    // BRANCH:    which slot to read for the condition
    //   Slots: 0 = memory_0.x, 1 = memory_0.y, 2 = memory_0.z, 3 = memory_0.w
    //          4 = memory_1.x, 5 = memory_1.y, 6 = memory_1.z, 7 = memory_1.w
    uint32_t state_key    = 0;

    // SET_STATE: value to write
    float    state_value  = 0.0f;

    // SPAWN: prefab name; PLAY_SOUND: sound asset name; LOOK_AT "player" literal
    std::string param_string;

    // BRANCH: step index to jump to when the condition is true (non-zero)
    uint32_t branch_target = 0;

    // EMIT_SIGNAL: signal strength to write to comms_signal
    float    signal_value = 1.0f;

    // SPAWN: world-space offset from entity position
    vec3     spawn_offset{0.0f};
};

// ---------------------------------------------------------------------------
// An ordered list of steps that executes serially
// ---------------------------------------------------------------------------
struct ActionSequence {
    std::string            name;
    uint32_t               id   = 0;
    bool                   loop = false;
    std::vector<ActionStep> steps;
};

// ---------------------------------------------------------------------------
// All sequences associated with one archetype (one .actions.xml file)
// ---------------------------------------------------------------------------
struct ActionSet {
    std::string                            archetype_name;
    std::vector<ActionSequence>            sequences;
    std::unordered_map<std::string, uint32_t> name_to_id; // name -> sequence id
};

// ---------------------------------------------------------------------------
// Pure: parse an .actions.xml file into an ActionSet.
// Returns err() with a descriptive message on any parse failure.
// ---------------------------------------------------------------------------
Result<ActionSet> parse_action_set(const std::filesystem::path& path);

// ---------------------------------------------------------------------------
// Runtime state for one entity that is currently executing a sequence
// ---------------------------------------------------------------------------
struct ActiveSequence {
    EntityID entity      = INVALID_ENTITY;
    uint32_t sequence_id = 0;
    uint32_t current_step = 0;
    float    step_timer   = 0.0f;  // time spent in the current step
    bool     waiting      = false; // true while step_timer < step duration
};

// ---------------------------------------------------------------------------
// ActionSystem
//
// Each frame the caller should:
//   1. Read BehaviorOutput buffers from the GPU (action_request / action_priority).
//   2. Call queue_action_request() for each entity that wrote a non-zero request.
//   3. Call tick(delta_time) to advance all active sequences.
//   4. Call consume_persist_writes() — write results to the GPU PersistState buffer.
//   5. Call consume_signal_writes()  — push comms_signal values to the GPU output buffer.
//   6. Call consume_sound_requests() — dispatch sound/voice events to the audio system.
//   7. Call consume_spawn_requests() — hand off spawning to the scene system.
//   8. Call consume_destroy_requests() — destroy entities via the entity manager.
//
// All mutation is expressed as plain data structs — no GPU handles here.
// ---------------------------------------------------------------------------

// A request to write a float into a persist state slot for one entity
struct PersistWrite {
    EntityID entity    = INVALID_ENTITY;
    uint32_t state_key = 0;    // same slot encoding as ActionStep::state_key
    float    value     = 0.0f;
};

// A request to write comms_signal back into the output buffer for one entity
struct SignalWrite {
    EntityID entity       = INVALID_ENTITY;
    float    signal_value = 0.0f;
};

// A request to play a sound/voice line for one entity
struct SoundRequest {
    EntityID    entity      = INVALID_ENTITY;
    std::string sound_name;   // asset name (from PLAY_SOUND param_string)
    uint32_t    sound_id  = 0; // numeric id if provided
};

// A request to spawn a prefab near an entity
struct SpawnRequest {
    EntityID    entity      = INVALID_ENTITY;
    std::string prefab_name;
    vec3        world_offset{0.0f}; // offset from entity position at call time
};

// An action request sourced from the GPU output buffer
struct ActionRequest {
    EntityID entity      = INVALID_ENTITY;
    uint32_t sequence_id = 0;
    float    priority    = 0.0f;
};

class ActionSystem {
public:
    ActionSystem() = default;

    // Non-copyable, movable
    ActionSystem(const ActionSystem&) = delete;
    ActionSystem& operator=(const ActionSystem&) = delete;
    ActionSystem(ActionSystem&&) = default;
    ActionSystem& operator=(ActionSystem&&) = default;

    // Register the action set for an archetype.
    // The archetype name must match the BehaviorOutput archetype name.
    void register_action_set(const std::string& archetype, ActionSet set);

    // Submit an action request (typically sourced from the GPU output buffer).
    // If the entity is already executing a sequence with equal or higher priority
    // that same frame the new request is silently dropped.
    void queue_action_request(const ActionRequest& request, const std::string& archetype);

    // Advance all active sequences by delta_time.
    // Call once per frame after queue_action_request() calls.
    void tick(float delta_time);

    // Consume pending persist-state writes (call after tick()).
    // Returns the list and clears the internal buffer.
    std::vector<PersistWrite> consume_persist_writes();

    // Consume pending signal writes (call after tick()).
    std::vector<SignalWrite> consume_signal_writes();

    // Consume pending sound requests (call after tick()).
    std::vector<SoundRequest> consume_sound_requests();

    // Consume pending spawn requests (call after tick()).
    std::vector<SpawnRequest> consume_spawn_requests();

    // Consume entities that requested self-destruction (call after tick()).
    std::vector<EntityID> consume_destroy_requests();

    // Check whether an entity is currently executing a sequence
    bool is_running(EntityID entity) const;

    // Stop a running sequence immediately (e.g. entity was killed externally)
    void cancel_sequence(EntityID entity);

    // Cancel all sequences
    void cancel_all();

private:
    // Look up a sequence by id within an archetype's action set
    const ActionSequence* find_sequence(const std::string& archetype,
                                        uint32_t sequence_id) const;

    // Execute one step immediately (non-timer steps) or begin a timed step.
    // Returns true if the step completed instantly (advance to next step).
    bool begin_step(ActiveSequence& active,
                    const ActionStep& step,
                    const std::string& archetype);

    // Registered action sets, keyed by archetype name
    std::unordered_map<std::string, ActionSet> action_sets_;

    // Entity -> archetype name (so tick() can look up sequences)
    std::unordered_map<EntityID, std::string> entity_archetype_;

    // Currently executing sequences
    std::vector<ActiveSequence> active_sequences_;

    // Pending output mutations (consumed each frame)
    std::vector<PersistWrite>  pending_persist_writes_;
    std::vector<SignalWrite>    pending_signal_writes_;
    std::vector<SoundRequest>   pending_sound_requests_;
    std::vector<SpawnRequest>   pending_spawn_requests_;
    std::vector<EntityID>       pending_destroy_requests_;
};

} // namespace odyssey::nadir
