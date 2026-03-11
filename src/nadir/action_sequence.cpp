#include "nadir/action_sequence.h"

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <algorithm>

namespace odyssey::nadir {

// ---------------------------------------------------------------------------
// Internal parse helpers
// ---------------------------------------------------------------------------

namespace {

// Parse "x y z" into a vec3; returns default_val on any failure.
vec3 parse_vec3_str(const char* str, vec3 default_val = vec3{0.0f}) {
    if (!str || str[0] == '\0') return default_val;
    std::istringstream iss(str);
    vec3 v = default_val;
    iss >> v.x >> v.y >> v.z;
    return v;
}

float parse_float_attr(const pugi::xml_attribute& attr, float fallback = 0.0f) {
    if (!attr) return fallback;
    return attr.as_float(fallback);
}

uint32_t parse_uint_attr(const pugi::xml_attribute& attr, uint32_t fallback = 0) {
    if (!attr) return fallback;
    return static_cast<uint32_t>(attr.as_uint(fallback));
}

// ---------------------------------------------------------------------------
// Parse a single <step> node into an ActionStep.
// The tag name determines the ActionType.
// Returns false (and logs a warning) for unknown tags.
// ---------------------------------------------------------------------------
bool parse_step(const pugi::xml_node& node, ActionStep& out_step) {
    std::string tag = node.name();

    if (tag == "move_to") {
        out_step.type            = ActionType::MOVE_TO;
        out_step.target_position = parse_vec3_str(
            node.attribute("position").as_string(""));
        return true;
    }

    if (tag == "wait") {
        out_step.type     = ActionType::WAIT;
        out_step.duration = parse_float_attr(node.attribute("duration"), 1.0f);
        return true;
    }

    if (tag == "play_anim") {
        out_step.type     = ActionType::PLAY_ANIM;
        out_step.anim_id  = parse_uint_attr(node.attribute("id"), 0);
        out_step.duration = parse_float_attr(node.attribute("duration"), 1.0f);
        return true;
    }

    if (tag == "play_sound") {
        out_step.type         = ActionType::PLAY_SOUND;
        out_step.param_string = node.attribute("name").as_string("");
        out_step.sound_id     = parse_uint_attr(node.attribute("id"), 0);
        return true;
    }

    if (tag == "set_state") {
        out_step.type        = ActionType::SET_STATE;
        out_step.state_key   = parse_uint_attr(node.attribute("key"), 0);
        out_step.state_value = parse_float_attr(node.attribute("value"), 0.0f);
        return true;
    }

    if (tag == "look_at") {
        out_step.type         = ActionType::LOOK_AT;
        // target may be "player" literal or a positional "x y z" string
        const char* target = node.attribute("target").as_string("");
        out_step.param_string = target; // keep raw string for caller interpretation
        // Try to also parse as position (will be zero if it's a name like "player")
        out_step.target_position = parse_vec3_str(target, vec3{0.0f});
        return true;
    }

    if (tag == "spawn") {
        out_step.type         = ActionType::SPAWN;
        out_step.param_string = node.attribute("prefab").as_string("");
        out_step.spawn_offset = parse_vec3_str(
            node.attribute("offset").as_string(""), vec3{0.0f});
        return true;
    }

    if (tag == "destroy_self") {
        out_step.type = ActionType::DESTROY_SELF;
        return true;
    }

    if (tag == "emit_signal") {
        out_step.type         = ActionType::EMIT_SIGNAL;
        out_step.signal_value = parse_float_attr(node.attribute("value"), 1.0f);
        return true;
    }

    if (tag == "branch") {
        out_step.type          = ActionType::BRANCH;
        out_step.state_key     = parse_uint_attr(node.attribute("key"), 0);
        out_step.branch_target = parse_uint_attr(node.attribute("target"), 0);
        return true;
    }

    // Unknown tag — warn but don't fail the whole parse
    spdlog::warn("ActionSequence: unknown step tag '<{}>'; skipping", tag);
    return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Pure: parse_action_set
// ---------------------------------------------------------------------------
Result<ActionSet> parse_action_set(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<ActionSet>::err(
            "Action set file not found: " + path.string());
    }

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        return Result<ActionSet>::err(
            "Failed to open action set file: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_string(content.c_str());
    if (!parse_result) {
        return Result<ActionSet>::err(
            std::string("XML parse error in '") + path.string() +
            "': " + parse_result.description());
    }

    auto root = doc.child("actions");
    if (!root) {
        return Result<ActionSet>::err(
            "Missing root <actions> element in: " + path.string());
    }

    ActionSet action_set;
    action_set.archetype_name = root.attribute("archetype").as_string("");
    if (action_set.archetype_name.empty()) {
        spdlog::warn("ActionSet '{}': missing archetype attribute", path.string());
    }

    // Iterate <sequence> children
    for (auto seq_node : root.children("sequence")) {
        ActionSequence seq;
        seq.name = seq_node.attribute("name").as_string("unnamed");
        seq.id   = parse_uint_attr(seq_node.attribute("id"), 0);
        seq.loop = seq_node.attribute("loop").as_bool(false);

        // Parse each step child
        for (auto step_node : seq_node.children()) {
            // Skip XML comment nodes and text nodes
            if (step_node.type() != pugi::node_element) continue;

            ActionStep step;
            if (parse_step(step_node, step)) {
                seq.steps.push_back(std::move(step));
            }
        }

        if (seq.steps.empty()) {
            spdlog::warn("ActionSet '{}': sequence '{}' (id={}) has no steps",
                         action_set.archetype_name, seq.name, seq.id);
        }

        // Register name -> id mapping
        action_set.name_to_id[seq.name] = seq.id;
        action_set.sequences.push_back(std::move(seq));
    }

    spdlog::info("Parsed action set for archetype '{}': {} sequences",
                 action_set.archetype_name,
                 action_set.sequences.size());

    return Result<ActionSet>::ok(std::move(action_set));
}

// ---------------------------------------------------------------------------
// ActionSystem — registration
// ---------------------------------------------------------------------------

void ActionSystem::register_action_set(const std::string& archetype, ActionSet set) {
    spdlog::info("ActionSystem: registered action set for archetype '{}' ({} sequences)",
                 archetype, set.sequences.size());
    action_sets_[archetype] = std::move(set);
}

// ---------------------------------------------------------------------------
// ActionSystem — queue a request from the GPU output buffer
// ---------------------------------------------------------------------------

void ActionSystem::queue_action_request(const ActionRequest& request,
                                        const std::string& archetype) {
    if (request.sequence_id == 0) return;
    if (request.entity == INVALID_ENTITY) return;

    // If already running a sequence, only interrupt for higher priority
    for (const auto& active : active_sequences_) {
        if (active.entity == request.entity) {
            // Already running — drop this request (lower or equal priority)
            return;
        }
    }

    // Record the archetype mapping so tick() can resolve the sequence
    entity_archetype_[request.entity] = archetype;

    ActiveSequence active;
    active.entity       = request.entity;
    active.sequence_id  = request.sequence_id;
    active.current_step = 0;
    active.step_timer   = 0.0f;
    active.waiting      = false;

    // Find the sequence to validate it exists before adding
    const ActionSequence* seq = find_sequence(archetype, request.sequence_id);
    if (!seq) {
        spdlog::warn("ActionSystem: archetype '{}' has no sequence id={}; dropping request",
                     archetype, request.sequence_id);
        return;
    }

    if (seq->steps.empty()) {
        spdlog::warn("ActionSystem: sequence id={} ('{}') has no steps; dropping",
                     request.sequence_id, seq->name);
        return;
    }

    active_sequences_.push_back(std::move(active));
    spdlog::debug("ActionSystem: started sequence '{}' (id={}) for entity {}",
                  seq->name, request.sequence_id, request.entity);
}

// ---------------------------------------------------------------------------
// ActionSystem — tick
// ---------------------------------------------------------------------------

void ActionSystem::tick(float delta_time) {
    // Iterate backwards so we can erase completed sequences safely
    for (int i = static_cast<int>(active_sequences_.size()) - 1; i >= 0; --i) {
        ActiveSequence& active = active_sequences_[static_cast<size_t>(i)];

        const std::string& archetype = entity_archetype_[active.entity];
        const ActionSequence* seq    = find_sequence(archetype, active.sequence_id);
        if (!seq) {
            // Sequence disappeared (shouldn't happen in normal operation)
            active_sequences_.erase(active_sequences_.begin() + i);
            continue;
        }

        // Advance through steps until we hit a blocking (timed) step
        bool sequence_alive = true;
        while (sequence_alive) {
            if (active.current_step >= seq->steps.size()) {
                // Reached end of sequence
                if (seq->loop) {
                    active.current_step = 0;
                    active.step_timer   = 0.0f;
                    active.waiting      = false;
                } else {
                    // Sequence complete — clean up
                    spdlog::debug("ActionSystem: sequence '{}' complete for entity {}",
                                  seq->name, active.entity);
                    active_sequences_.erase(active_sequences_.begin() + i);
                    sequence_alive = false;
                }
                break;
            }

            const ActionStep& step = seq->steps[active.current_step];

            if (!active.waiting) {
                // Begin this step
                bool instant = begin_step(active, step, archetype);
                if (instant) {
                    // Instant step — move immediately to next
                    ++active.current_step;
                    active.step_timer = 0.0f;
                    continue;
                } else {
                    // Timed step — wait for duration
                    active.waiting = true;
                }
            }

            // Advance timer for timed step
            active.step_timer += delta_time;
            float duration = step.duration;

            if (active.step_timer >= duration) {
                // Timed step complete
                active.step_timer = 0.0f;
                active.waiting    = false;
                ++active.current_step;
            } else {
                // Still waiting — stop advancing this frame
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ActionSystem — begin_step
// Returns true if the step is instant (no timer needed).
// Returns false if the step requires a timer (WAIT, PLAY_ANIM, MOVE_TO).
// ---------------------------------------------------------------------------

bool ActionSystem::begin_step(ActiveSequence& active,
                               const ActionStep& step,
                               const std::string& archetype) {
    switch (step.type) {

    case ActionType::MOVE_TO:
        // Movement is resolved externally (the game reads the active sequence's
        // current step to know the destination). We treat MOVE_TO as a timed
        // step with an implicit or explicit duration.
        // If duration == 0 we use a large sentinel so the engine never auto-advances;
        // real arrival detection should call cancel_sequence() externally.
        // For simplicity in this MVI: if duration > 0 use it, otherwise block.
        return false; // timed

    case ActionType::WAIT:
        return false; // timed

    case ActionType::PLAY_ANIM:
        // Animation is requested — the game reads current step anim_id.
        return false; // timed (duration = animation length)

    case ActionType::PLAY_SOUND: {
        // Fire-and-forget: emit a sound request that the game loop dispatches
        // to the audio system this frame.
        SoundRequest sr;
        sr.entity     = active.entity;
        sr.sound_name = step.param_string;
        sr.sound_id   = step.sound_id;
        pending_sound_requests_.push_back(std::move(sr));
        return true; // instant step
    }

    case ActionType::SET_STATE: {
        PersistWrite pw;
        pw.entity    = active.entity;
        pw.state_key = step.state_key;
        pw.value     = step.state_value;
        pending_persist_writes_.push_back(pw);
        return true;
    }

    case ActionType::LOOK_AT:
        // Face direction is resolved externally via active step inspection.
        return false; // timed — use duration if present, else treat as instant

    case ActionType::SPAWN: {
        SpawnRequest sr;
        sr.entity       = active.entity;
        sr.prefab_name  = step.param_string;
        sr.world_offset = step.spawn_offset;
        pending_spawn_requests_.push_back(std::move(sr));
        return true;
    }

    case ActionType::DESTROY_SELF:
        pending_destroy_requests_.push_back(active.entity);
        return true;

    case ActionType::EMIT_SIGNAL: {
        SignalWrite sw;
        sw.entity       = active.entity;
        sw.signal_value = step.signal_value;
        pending_signal_writes_.push_back(sw);
        return true;
    }

    case ActionType::BRANCH: {
        // Branch is evaluated via a PersistWrite read-back.
        // At this point the CPU has no direct access to the GPU buffer value,
        // but the Nadir shader wrote action_request after reading persist state,
        // so we must rely on any prior SET_STATE mutations made this session.
        // Simple approach: find the most recent PersistWrite for this entity/key
        // and branch on that; default to NOT branching if no write was seen.
        float key_value = 0.0f;
        for (const auto& pw : pending_persist_writes_) {
            if (pw.entity == active.entity && pw.state_key == step.state_key) {
                key_value = pw.value;
            }
        }
        if (key_value != 0.0f) {
            // Jump to branch_target step index
            active.current_step = step.branch_target;
            // Clamp to valid range using the archetype already in scope
            const ActionSequence* seq = find_sequence(archetype, active.sequence_id);
            if (seq && active.current_step >= seq->steps.size()) {
                active.current_step = static_cast<uint32_t>(seq->steps.size());
            }
            active.step_timer = 0.0f;
            active.waiting    = false;
        }
        return true; // branch itself is instant
    }

    }

    return true; // unreachable
}

// ---------------------------------------------------------------------------
// ActionSystem — output consumers
// ---------------------------------------------------------------------------

std::vector<PersistWrite> ActionSystem::consume_persist_writes() {
    std::vector<PersistWrite> out;
    out.swap(pending_persist_writes_);
    return out;
}

std::vector<SignalWrite> ActionSystem::consume_signal_writes() {
    std::vector<SignalWrite> out;
    out.swap(pending_signal_writes_);
    return out;
}

std::vector<SoundRequest> ActionSystem::consume_sound_requests() {
    std::vector<SoundRequest> out;
    out.swap(pending_sound_requests_);
    return out;
}

std::vector<SpawnRequest> ActionSystem::consume_spawn_requests() {
    std::vector<SpawnRequest> out;
    out.swap(pending_spawn_requests_);
    return out;
}

std::vector<EntityID> ActionSystem::consume_destroy_requests() {
    std::vector<EntityID> out;
    out.swap(pending_destroy_requests_);
    return out;
}

// ---------------------------------------------------------------------------
// ActionSystem — query / control
// ---------------------------------------------------------------------------

bool ActionSystem::is_running(EntityID entity) const {
    for (const auto& active : active_sequences_) {
        if (active.entity == entity) return true;
    }
    return false;
}

void ActionSystem::cancel_sequence(EntityID entity) {
    auto it = std::find_if(
        active_sequences_.begin(), active_sequences_.end(),
        [entity](const ActiveSequence& a) { return a.entity == entity; });

    if (it != active_sequences_.end()) {
        spdlog::debug("ActionSystem: cancelled sequence for entity {}", entity);
        active_sequences_.erase(it);
    }
}

void ActionSystem::cancel_all() {
    active_sequences_.clear();
    spdlog::debug("ActionSystem: all sequences cancelled");
}

// ---------------------------------------------------------------------------
// ActionSystem — internal helpers
// ---------------------------------------------------------------------------

const ActionSequence* ActionSystem::find_sequence(const std::string& archetype,
                                                   uint32_t sequence_id) const {
    auto it = action_sets_.find(archetype);
    if (it == action_sets_.end()) return nullptr;

    const ActionSet& set = it->second;
    for (const auto& seq : set.sequences) {
        if (seq.id == sequence_id) return &seq;
    }
    return nullptr;
}

} // namespace odyssey::nadir
