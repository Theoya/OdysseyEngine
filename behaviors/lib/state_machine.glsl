// state_machine.glsl — Lightweight state machine helpers for behavior shaders

#ifndef STATE_MACHINE_GLSL
#define STATE_MACHINE_GLSL

// State constants (common states)
const uint STATE_IDLE    = 0u;
const uint STATE_PATROL  = 1u;
const uint STATE_ALERT   = 2u;
const uint STATE_COMBAT  = 3u;
const uint STATE_FLEE    = 4u;
const uint STATE_DEAD    = 5u;
const uint STATE_SEARCH  = 6u;
const uint STATE_COVER   = 7u;

// Check if agent is in a specific state
bool is_state(uint current, uint target) {
    return current == target;
}

// Scored state transition — returns new state based on highest-scoring transition
// Uses step() for branchless comparison
uint transition_scored(uint current, float score_combat, float score_flee, float score_patrol, float threshold) {
    // Only transition if score exceeds threshold (hysteresis)
    float max_score = max(max(score_combat, score_flee), score_patrol);
    if (max_score < threshold) return current;

    // Pick highest scoring state
    if (score_combat >= score_flee && score_combat >= score_patrol) return STATE_COMBAT;
    if (score_flee >= score_combat && score_flee >= score_patrol) return STATE_FLEE;
    return STATE_PATROL;
}

// Two-state transition (branchless)
uint transition_two(uint current, uint state_a, uint state_b, float score_a, float score_b, float threshold) {
    float max_s = max(score_a, score_b);
    if (max_s < threshold) return current;
    return (score_a >= score_b) ? state_a : state_b;
}

// Timer update: increment timer by delta_time
float timer_tick(float timer, float dt) {
    return timer + dt;
}

// Timer check: returns 1.0 if timer >= duration, 0.0 otherwise
float timer_elapsed(float timer, float duration) {
    return step(duration, timer);
}

// Timer reset on state change
float timer_on_transition(float timer, uint prev_state, uint new_state) {
    return (prev_state != new_state) ? 0.0 : timer;
}

// Cooldown system: returns remaining cooldown
float cooldown_tick(float cooldown, float dt) {
    return max(0.0, cooldown - dt);
}

// Start cooldown
float cooldown_start(float duration) {
    return duration;
}

// Check cooldown ready
bool cooldown_is_ready(float cooldown) {
    return cooldown <= 0.0;
}

// State duration modifier: weight that increases over time in a state
float state_urgency(float state_timer, float ramp_duration) {
    return clamp(state_timer / max(ramp_duration, 0.001), 0.0, 1.0);
}

// Hysteresis: make it harder to leave current state (add bonus to current)
float hysteresis_bonus(uint current_state, uint candidate_state, float bonus) {
    return (current_state == candidate_state) ? bonus : 0.0;
}

#endif // STATE_MACHINE_GLSL
