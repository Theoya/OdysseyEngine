#include "net/replication.h"
#include <cmath>
#include <algorithm>

namespace odyssey::net {

// ─── Delta compression ───────────────────────────────────────────────────────

std::vector<EntityDelta> compute_snapshot_delta(
    const std::vector<EntitySnapshot>& old_snap,
    const std::vector<EntitySnapshot>& new_snap,
    float position_threshold,
    float rotation_threshold,
    float health_threshold
) {
    std::vector<EntityDelta> deltas;

    // Build lookup from old snapshot
    std::unordered_map<EntityID, const EntitySnapshot*> old_map;
    for (const auto& e : old_snap) {
        old_map[e.entity_id] = &e;
    }

    for (const auto& new_entity : new_snap) {
        auto it = old_map.find(new_entity.entity_id);

        EntityDelta delta;
        delta.entity_id = new_entity.entity_id;
        delta.position = new_entity.position;
        delta.rotation = new_entity.rotation;
        delta.health = new_entity.health;
        delta.state = new_entity.state;

        if (it == old_map.end()) {
            // New entity: everything changed
            delta.position_changed = true;
            delta.rotation_changed = true;
            delta.health_changed = true;
            delta.state_changed = true;
            deltas.push_back(delta);
        } else {
            const auto& old_entity = *it->second;
            bool any_changed = false;

            // Position delta
            float pos_dist = glm::length(new_entity.position - old_entity.position);
            if (pos_dist > position_threshold) {
                delta.position_changed = true;
                any_changed = true;
            }

            // Rotation delta
            float rot_diff = 1.0f - std::abs(glm::dot(new_entity.rotation, old_entity.rotation));
            if (rot_diff > rotation_threshold) {
                delta.rotation_changed = true;
                any_changed = true;
            }

            // Health delta
            if (std::abs(new_entity.health - old_entity.health) > health_threshold) {
                delta.health_changed = true;
                any_changed = true;
            }

            // State delta
            if (new_entity.state != old_entity.state) {
                delta.state_changed = true;
                any_changed = true;
            }

            if (any_changed) {
                deltas.push_back(delta);
            }
        }
    }

    return deltas;
}

std::vector<EntitySnapshot> apply_snapshot_delta(
    const std::vector<EntitySnapshot>& base,
    const std::vector<EntityDelta>& deltas
) {
    // Start with a copy of the base
    std::vector<EntitySnapshot> result = base;

    // Build map for quick lookup by entity_id
    std::unordered_map<EntityID, size_t> index_map;
    for (size_t i = 0; i < result.size(); ++i) {
        index_map[result[i].entity_id] = i;
    }

    for (const auto& delta : deltas) {
        auto it = index_map.find(delta.entity_id);

        if (it != index_map.end()) {
            // Update existing entity
            auto& entity = result[it->second];
            if (delta.position_changed) entity.position = delta.position;
            if (delta.rotation_changed) entity.rotation = delta.rotation;
            if (delta.health_changed) entity.health = delta.health;
            if (delta.state_changed) entity.state = delta.state;
        } else {
            // New entity from delta
            EntitySnapshot new_entity;
            new_entity.entity_id = delta.entity_id;
            new_entity.position = delta.position;
            new_entity.rotation = delta.rotation;
            new_entity.health = delta.health;
            new_entity.state = delta.state;
            index_map[delta.entity_id] = result.size();
            result.push_back(new_entity);
        }
    }

    return result;
}

// ─── Interpolation ───────────────────────────────────────────────────────────

std::vector<EntitySnapshot> interpolate_snapshots(
    const std::vector<EntitySnapshot>& from,
    const std::vector<EntitySnapshot>& to,
    float t
) {
    // Clamp t
    t = std::clamp(t, 0.0f, 1.0f);

    // Build lookup for "to" snapshot
    std::unordered_map<EntityID, const EntitySnapshot*> to_map;
    for (const auto& e : to) {
        to_map[e.entity_id] = &e;
    }

    std::vector<EntitySnapshot> result;
    result.reserve(std::max(from.size(), to.size()));

    // Track which entities from "to" we've processed
    std::unordered_map<EntityID, bool> processed;

    // Interpolate entities present in "from"
    for (const auto& from_entity : from) {
        auto it = to_map.find(from_entity.entity_id);

        if (it != to_map.end()) {
            const auto& to_entity = *it->second;
            EntitySnapshot interp;
            interp.entity_id = from_entity.entity_id;
            interp.position = glm::mix(from_entity.position, to_entity.position, t);
            interp.rotation = glm::slerp(from_entity.rotation, to_entity.rotation, t);
            interp.health = glm::mix(from_entity.health, to_entity.health, t);
            interp.speed = glm::mix(from_entity.speed, to_entity.speed, t);
            // Discrete values: use "from" for t < 0.5, "to" for t >= 0.5
            interp.state = (t < 0.5f) ? from_entity.state : to_entity.state;
            interp.flags = (t < 0.5f) ? from_entity.flags : to_entity.flags;
            result.push_back(interp);
            processed[from_entity.entity_id] = true;
        } else {
            // Entity only in "from" — include it (will fade out)
            result.push_back(from_entity);
            processed[from_entity.entity_id] = true;
        }
    }

    // Add entities only in "to" (newly spawned)
    for (const auto& to_entity : to) {
        if (processed.find(to_entity.entity_id) == processed.end()) {
            result.push_back(to_entity);
        }
    }

    return result;
}

// ─── Delta serialization ─────────────────────────────────────────────────────

std::vector<uint8_t> serialize_delta_snapshot(
    const std::vector<EntityDelta>& deltas,
    uint16_t sequence,
    uint16_t ack,
    uint32_t ack_bits
) {
    PacketWriter writer;

    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.sequence = sequence;
    header.ack = ack;
    header.ack_bits = ack_bits;
    header.type = PacketType::DELTA_SNAPSHOT;
    writer.write_header(header);

    uint32_t count = static_cast<uint32_t>(deltas.size());
    writer.write_u32(count);

    for (const auto& delta : deltas) {
        writer.write_u32(delta.entity_id);

        // Pack change flags into a single byte
        uint8_t flags = 0;
        if (delta.position_changed) flags |= 0x01;
        if (delta.rotation_changed) flags |= 0x02;
        if (delta.health_changed)   flags |= 0x04;
        if (delta.state_changed)    flags |= 0x08;
        writer.write_u8(flags);

        if (delta.position_changed) writer.write_vec3(delta.position);
        if (delta.rotation_changed) writer.write_quat(delta.rotation);
        if (delta.health_changed)   writer.write_float(delta.health);
        if (delta.state_changed)    writer.write_u8(delta.state);
    }

    return std::vector<uint8_t>(writer.data(), writer.data() + writer.size());
}

std::vector<EntityDelta> deserialize_delta_snapshot(const uint8_t* data, size_t size) {
    PacketReader reader(data, size);
    reader.read_header(); // skip header

    uint32_t count = reader.read_u32();
    std::vector<EntityDelta> deltas;
    deltas.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        EntityDelta delta;
        delta.entity_id = reader.read_u32();

        uint8_t flags = reader.read_u8();
        delta.position_changed = (flags & 0x01) != 0;
        delta.rotation_changed = (flags & 0x02) != 0;
        delta.health_changed   = (flags & 0x04) != 0;
        delta.state_changed    = (flags & 0x08) != 0;

        if (delta.position_changed) delta.position = reader.read_vec3();
        if (delta.rotation_changed) delta.rotation = reader.read_quat();
        if (delta.health_changed)   delta.health = reader.read_float();
        if (delta.state_changed)    delta.state = reader.read_u8();

        deltas.push_back(delta);
    }

    return deltas;
}

// ─── SnapshotBuffer ──────────────────────────────────────────────────────────

void SnapshotBuffer::add_snapshot(uint16_t sequence, float timestamp, std::vector<EntitySnapshot> snapshot) {
    TimedSnapshot ts;
    ts.sequence = sequence;
    ts.timestamp = timestamp;
    ts.entities = std::move(snapshot);

    // Insert in order by timestamp
    auto it = std::lower_bound(snapshots_.begin(), snapshots_.end(), ts,
        [](const TimedSnapshot& a, const TimedSnapshot& b) {
            return a.timestamp < b.timestamp;
        });
    snapshots_.insert(it, std::move(ts));
}

std::vector<EntitySnapshot> SnapshotBuffer::get_interpolated(float render_time) const {
    if (snapshots_.empty()) return {};

    // Find the two snapshots to interpolate between
    // We want: snapshots_[i].timestamp <= render_time < snapshots_[i+1].timestamp
    if (render_time <= snapshots_.front().timestamp) {
        return snapshots_.front().entities;
    }
    if (render_time >= snapshots_.back().timestamp) {
        return snapshots_.back().entities;
    }

    for (size_t i = 0; i + 1 < snapshots_.size(); ++i) {
        if (snapshots_[i].timestamp <= render_time &&
            render_time < snapshots_[i + 1].timestamp) {
            float duration = snapshots_[i + 1].timestamp - snapshots_[i].timestamp;
            float t = (duration > 0.0001f) ?
                (render_time - snapshots_[i].timestamp) / duration : 0.0f;
            return interpolate_snapshots(snapshots_[i].entities,
                                          snapshots_[i + 1].entities, t);
        }
    }

    return snapshots_.back().entities;
}

void SnapshotBuffer::prune(float oldest_time) {
    snapshots_.erase(
        std::remove_if(snapshots_.begin(), snapshots_.end(),
            [oldest_time](const TimedSnapshot& ts) {
                return ts.timestamp < oldest_time;
            }),
        snapshots_.end()
    );
}

} // namespace odyssey::net
