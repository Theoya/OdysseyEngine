#pragma once
#include "net/protocol.h"
#include "core/types.h"
#include <vector>
#include <unordered_map>

namespace odyssey::net {

// Pure: compute delta between two snapshots
struct EntityDelta {
    EntityID entity_id;
    bool position_changed = false;
    bool rotation_changed = false;
    bool health_changed = false;
    bool state_changed = false;
    vec3 position;
    quat rotation;
    float health;
    uint8_t state;
};

// Pure: compute deltas between old and new snapshots
std::vector<EntityDelta> compute_snapshot_delta(
    const std::vector<EntitySnapshot>& old_snap,
    const std::vector<EntitySnapshot>& new_snap,
    float position_threshold = 0.01f,
    float rotation_threshold = 0.001f,
    float health_threshold = 0.1f
);

// Pure: apply deltas to a base snapshot
std::vector<EntitySnapshot> apply_snapshot_delta(
    const std::vector<EntitySnapshot>& base,
    const std::vector<EntityDelta>& deltas
);

// Pure: interpolate between two snapshots for smooth rendering
std::vector<EntitySnapshot> interpolate_snapshots(
    const std::vector<EntitySnapshot>& from,
    const std::vector<EntitySnapshot>& to,
    float t  // 0.0 = from, 1.0 = to
);

// Pure: serialize/deserialize deltas
std::vector<uint8_t> serialize_delta_snapshot(
    const std::vector<EntityDelta>& deltas,
    uint16_t sequence,
    uint16_t ack,
    uint32_t ack_bits
);

std::vector<EntityDelta> deserialize_delta_snapshot(const uint8_t* data, size_t size);

// Snapshot buffer for interpolation
class SnapshotBuffer {
public:
    void add_snapshot(uint16_t sequence, float timestamp, std::vector<EntitySnapshot> snapshot);

    // Get interpolated snapshot at a given time (render time = server time - interp_delay)
    std::vector<EntitySnapshot> get_interpolated(float render_time) const;

    // Prune old snapshots
    void prune(float oldest_time);

    size_t size() const { return snapshots_.size(); }

private:
    struct TimedSnapshot {
        uint16_t sequence;
        float timestamp;
        std::vector<EntitySnapshot> entities;
    };
    std::vector<TimedSnapshot> snapshots_;
};

} // namespace odyssey::net
