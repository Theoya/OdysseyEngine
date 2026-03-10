// blackboard.glsl — Per-agent persistent memory read/write helpers
// Uses AgentPersistGPU.memory_0 and memory_1 (vec4 each = 8 floats of storage)

// Read float from memory slot (0-7)
float bb_read_float(uint idx, uint slot) {
    if (slot < 4u) {
        vec4 mem = persist[idx].memory_0;
        if (slot == 0u) return mem.x;
        if (slot == 1u) return mem.y;
        if (slot == 2u) return mem.z;
        return mem.w;
    } else {
        vec4 mem = persist[idx].memory_1;
        slot -= 4u;
        if (slot == 0u) return mem.x;
        if (slot == 1u) return mem.y;
        if (slot == 2u) return mem.z;
        return mem.w;
    }
}

// Write float to memory slot (0-7)
// NOTE: Since persist is read/write, we can write directly
void bb_write_float(uint idx, uint slot, float value) {
    if (slot < 4u) {
        if (slot == 0u) persist[idx].memory_0.x = value;
        else if (slot == 1u) persist[idx].memory_0.y = value;
        else if (slot == 2u) persist[idx].memory_0.z = value;
        else persist[idx].memory_0.w = value;
    } else {
        slot -= 4u;
        if (slot == 0u) persist[idx].memory_1.x = value;
        else if (slot == 1u) persist[idx].memory_1.y = value;
        else if (slot == 2u) persist[idx].memory_1.z = value;
        else persist[idx].memory_1.w = value;
    }
}

// Store a vec3 in memory slots 0-2 (or 4-6)
void bb_write_vec3(uint idx, uint start_slot, vec3 value) {
    bb_write_float(idx, start_slot, value.x);
    bb_write_float(idx, start_slot + 1u, value.y);
    bb_write_float(idx, start_slot + 2u, value.z);
}

// Read a vec3 from memory slots
vec3 bb_read_vec3(uint idx, uint start_slot) {
    return vec3(
        bb_read_float(idx, start_slot),
        bb_read_float(idx, start_slot + 1u),
        bb_read_float(idx, start_slot + 2u)
    );
}

// Store last known player position
void bb_store_last_player_pos(uint idx, vec3 pos) {
    bb_write_vec3(idx, 0u, pos);
}

// Recall last known player position
vec3 bb_recall_last_player_pos(uint idx) {
    return bb_read_vec3(idx, 0u);
}

// Store threat level in slot 3
void bb_store_threat(uint idx, float threat) {
    bb_write_float(idx, 3u, threat);
}

float bb_recall_threat(uint idx) {
    return bb_read_float(idx, 3u);
}

// Store home/patrol position in slots 4-6
void bb_store_home_pos(uint idx, vec3 pos) {
    bb_write_vec3(idx, 4u, pos);
}

vec3 bb_recall_home_pos(uint idx) {
    return bb_read_vec3(idx, 4u);
}

// Generic counter in slot 7
void bb_increment_counter(uint idx) {
    float val = bb_read_float(idx, 7u);
    bb_write_float(idx, 7u, val + 1.0);
}

float bb_read_counter(uint idx) {
    return bb_read_float(idx, 7u);
}

void bb_reset_counter(uint idx) {
    bb_write_float(idx, 7u, 0.0);
}
