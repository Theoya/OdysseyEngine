#include <gtest/gtest.h>
#include "net/protocol.h"
#include "net/replication.h"
#include <cstring>
#include <cmath>

using namespace odyssey;
using namespace odyssey::net;

// =============================================================================
// PacketWriter / PacketReader round-trip tests
// =============================================================================

TEST(PacketRoundTrip, U8) {
    PacketWriter writer;
    writer.write_u8(0);
    writer.write_u8(127);
    writer.write_u8(255);

    PacketReader reader(writer.data(), writer.size());
    EXPECT_EQ(reader.read_u8(), 0);
    EXPECT_EQ(reader.read_u8(), 127);
    EXPECT_EQ(reader.read_u8(), 255);
    EXPECT_FALSE(reader.has_data());
}

TEST(PacketRoundTrip, U16) {
    PacketWriter writer;
    writer.write_u16(0);
    writer.write_u16(1234);
    writer.write_u16(65535);

    PacketReader reader(writer.data(), writer.size());
    EXPECT_EQ(reader.read_u16(), 0);
    EXPECT_EQ(reader.read_u16(), 1234);
    EXPECT_EQ(reader.read_u16(), 65535);
    EXPECT_FALSE(reader.has_data());
}

TEST(PacketRoundTrip, U32) {
    PacketWriter writer;
    writer.write_u32(0);
    writer.write_u32(123456789);
    writer.write_u32(0xDEADBEEF);

    PacketReader reader(writer.data(), writer.size());
    EXPECT_EQ(reader.read_u32(), 0u);
    EXPECT_EQ(reader.read_u32(), 123456789u);
    EXPECT_EQ(reader.read_u32(), 0xDEADBEEFu);
    EXPECT_FALSE(reader.has_data());
}

TEST(PacketRoundTrip, Float) {
    PacketWriter writer;
    writer.write_float(0.0f);
    writer.write_float(3.14159f);
    writer.write_float(-42.5f);
    writer.write_float(1e10f);

    PacketReader reader(writer.data(), writer.size());
    EXPECT_FLOAT_EQ(reader.read_float(), 0.0f);
    EXPECT_FLOAT_EQ(reader.read_float(), 3.14159f);
    EXPECT_FLOAT_EQ(reader.read_float(), -42.5f);
    EXPECT_FLOAT_EQ(reader.read_float(), 1e10f);
    EXPECT_FALSE(reader.has_data());
}

TEST(PacketRoundTrip, Vec3) {
    vec3 original{1.5f, -2.3f, 100.0f};

    PacketWriter writer;
    writer.write_vec3(original);

    PacketReader reader(writer.data(), writer.size());
    vec3 result = reader.read_vec3();

    EXPECT_FLOAT_EQ(result.x, original.x);
    EXPECT_FLOAT_EQ(result.y, original.y);
    EXPECT_FLOAT_EQ(result.z, original.z);
    EXPECT_FALSE(reader.has_data());
}

TEST(PacketRoundTrip, Quat) {
    quat original = glm::normalize(quat{0.7f, 0.1f, 0.3f, 0.5f});

    PacketWriter writer;
    writer.write_quat(original);

    PacketReader reader(writer.data(), writer.size());
    quat result = reader.read_quat();

    EXPECT_NEAR(result.w, original.w, 1e-5f);
    EXPECT_NEAR(result.x, original.x, 1e-5f);
    EXPECT_NEAR(result.y, original.y, 1e-5f);
    EXPECT_NEAR(result.z, original.z, 1e-5f);
    EXPECT_FALSE(reader.has_data());
}

TEST(PacketRoundTrip, String) {
    const char* original = "Hello, World!";

    PacketWriter writer;
    writer.write_string(original, 32);

    PacketReader reader(writer.data(), writer.size());
    char result[32] = {};
    reader.read_string(result, 32);

    EXPECT_STREQ(result, original);
    EXPECT_FALSE(reader.has_data());
}

TEST(PacketRoundTrip, StringTruncation) {
    const char* long_str = "This is a very long string that exceeds the maximum allowed length";

    PacketWriter writer;
    writer.write_string(long_str, 16);

    PacketReader reader(writer.data(), writer.size());
    char result[16] = {};
    reader.read_string(result, 16);

    // Should be truncated to first 15 chars + null terminator area
    EXPECT_EQ(std::strlen(result), 15u); // "This is a very " minus trailing space = 15
}

TEST(PacketRoundTrip, Header) {
    PacketHeader original;
    original.protocol_id = PROTOCOL_ID;
    original.sequence = 42;
    original.ack = 40;
    original.ack_bits = 0x0000000F;
    original.type = PacketType::SNAPSHOT;

    PacketWriter writer;
    writer.write_header(original);

    PacketReader reader(writer.data(), writer.size());
    PacketHeader result = reader.read_header();

    EXPECT_EQ(result.protocol_id, PROTOCOL_ID);
    EXPECT_EQ(result.sequence, 42);
    EXPECT_EQ(result.ack, 40);
    EXPECT_EQ(result.ack_bits, 0x0000000Fu);
    EXPECT_EQ(result.type, PacketType::SNAPSHOT);
}

TEST(PacketRoundTrip, MixedTypes) {
    PacketWriter writer;
    writer.write_u8(42);
    writer.write_float(3.14f);
    writer.write_u32(999);
    writer.write_vec3(vec3{1, 2, 3});
    writer.write_u16(7777);

    PacketReader reader(writer.data(), writer.size());
    EXPECT_EQ(reader.read_u8(), 42);
    EXPECT_FLOAT_EQ(reader.read_float(), 3.14f);
    EXPECT_EQ(reader.read_u32(), 999u);
    vec3 v = reader.read_vec3();
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
    EXPECT_EQ(reader.read_u16(), 7777);
    EXPECT_FALSE(reader.has_data());
}

// =============================================================================
// Connect payload serialize/deserialize
// =============================================================================

TEST(ConnectPayload, RoundTrip) {
    ConnectPayload original;
    std::strncpy(original.player_name, "TestPlayer", sizeof(original.player_name) - 1);
    original.version = PROTOCOL_VERSION;

    auto packet = serialize_connect_request(original, 1);
    EXPECT_GT(packet.size(), 0u);
    EXPECT_LE(packet.size(), MAX_PACKET_SIZE);

    // Verify header
    auto header = deserialize_header(packet.data(), packet.size());
    EXPECT_EQ(header.protocol_id, PROTOCOL_ID);
    EXPECT_EQ(header.type, PacketType::CONNECT_REQUEST);
    EXPECT_EQ(header.sequence, 1);

    // Verify payload
    auto result = deserialize_connect(packet.data(), packet.size());
    EXPECT_STREQ(result.player_name, "TestPlayer");
    EXPECT_EQ(result.version, PROTOCOL_VERSION);
}

TEST(ConnectPayload, EmptyName) {
    ConnectPayload original;
    original.player_name[0] = '\0';
    original.version = PROTOCOL_VERSION;

    auto packet = serialize_connect_request(original, 0);
    auto result = deserialize_connect(packet.data(), packet.size());
    EXPECT_STREQ(result.player_name, "");
    EXPECT_EQ(result.version, PROTOCOL_VERSION);
}

// =============================================================================
// Input payload serialize/deserialize
// =============================================================================

TEST(InputPayload, RoundTrip) {
    InputPayload original;
    original.forward = 1.0f;
    original.strafe = -0.5f;
    original.look_yaw = 3.14f;
    original.look_pitch = -0.5f;
    original.buttons = 0b00001011; // shoot + jump + interact
    original.sequence = 42;

    auto packet = serialize_input(original, 10, 8, 0xFF);
    EXPECT_LE(packet.size(), MAX_PACKET_SIZE);

    auto header = deserialize_header(packet.data(), packet.size());
    EXPECT_EQ(header.type, PacketType::INPUT);
    EXPECT_EQ(header.sequence, 10);
    EXPECT_EQ(header.ack, 8);
    EXPECT_EQ(header.ack_bits, 0xFFu);

    auto result = deserialize_input(packet.data(), packet.size());
    EXPECT_FLOAT_EQ(result.forward, 1.0f);
    EXPECT_FLOAT_EQ(result.strafe, -0.5f);
    EXPECT_FLOAT_EQ(result.look_yaw, 3.14f);
    EXPECT_FLOAT_EQ(result.look_pitch, -0.5f);
    EXPECT_EQ(result.buttons, 0b00001011);
    EXPECT_EQ(result.sequence, 42u);
}

TEST(InputPayload, ZeroInput) {
    InputPayload original; // all defaults = zero

    auto packet = serialize_input(original, 0, 0, 0);
    auto result = deserialize_input(packet.data(), packet.size());
    EXPECT_FLOAT_EQ(result.forward, 0.0f);
    EXPECT_FLOAT_EQ(result.strafe, 0.0f);
    EXPECT_FLOAT_EQ(result.look_yaw, 0.0f);
    EXPECT_FLOAT_EQ(result.look_pitch, 0.0f);
    EXPECT_EQ(result.buttons, 0);
    EXPECT_EQ(result.sequence, 0u);
}

// =============================================================================
// Snapshot serialize/deserialize
// =============================================================================

TEST(Snapshot, RoundTripSingleEntity) {
    std::vector<EntitySnapshot> entities;
    EntitySnapshot e;
    e.entity_id = 1;
    e.position = vec3{10.0f, 20.0f, 30.0f};
    e.rotation = glm::normalize(quat{1.0f, 0.0f, 0.0f, 0.0f});
    e.health = 75.0f;
    e.speed = 5.5f;
    e.state = 3;
    e.flags = 0x01;
    entities.push_back(e);

    auto packet = serialize_snapshot(entities, 100, 98, 0xF0F0);
    EXPECT_LE(packet.size(), MAX_PACKET_SIZE);

    auto header = deserialize_header(packet.data(), packet.size());
    EXPECT_EQ(header.type, PacketType::SNAPSHOT);
    EXPECT_EQ(header.sequence, 100);

    auto result = deserialize_snapshot(packet.data(), packet.size());
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].entity_id, 1u);
    EXPECT_FLOAT_EQ(result[0].position.x, 10.0f);
    EXPECT_FLOAT_EQ(result[0].position.y, 20.0f);
    EXPECT_FLOAT_EQ(result[0].position.z, 30.0f);
    EXPECT_FLOAT_EQ(result[0].health, 75.0f);
    EXPECT_FLOAT_EQ(result[0].speed, 5.5f);
    EXPECT_EQ(result[0].state, 3);
    EXPECT_EQ(result[0].flags, 0x01);
}

TEST(Snapshot, RoundTripMultipleEntities) {
    std::vector<EntitySnapshot> entities;
    for (uint32_t i = 0; i < 10; ++i) {
        EntitySnapshot e;
        e.entity_id = i;
        e.position = vec3{static_cast<float>(i), static_cast<float>(i * 2), static_cast<float>(i * 3)};
        e.rotation = quat{1, 0, 0, 0};
        e.health = 100.0f - static_cast<float>(i) * 10.0f;
        e.speed = static_cast<float>(i);
        e.state = static_cast<uint8_t>(i % 4);
        e.flags = static_cast<uint8_t>(i & 0x03);
        entities.push_back(e);
    }

    auto packet = serialize_snapshot(entities, 50, 48, 0);
    auto result = deserialize_snapshot(packet.data(), packet.size());

    ASSERT_EQ(result.size(), 10u);
    for (uint32_t i = 0; i < 10; ++i) {
        EXPECT_EQ(result[i].entity_id, i);
        EXPECT_FLOAT_EQ(result[i].position.x, static_cast<float>(i));
        EXPECT_FLOAT_EQ(result[i].health, 100.0f - static_cast<float>(i) * 10.0f);
        EXPECT_EQ(result[i].state, static_cast<uint8_t>(i % 4));
    }
}

TEST(Snapshot, EmptySnapshot) {
    std::vector<EntitySnapshot> empty;
    auto packet = serialize_snapshot(empty, 0, 0, 0);
    auto result = deserialize_snapshot(packet.data(), packet.size());
    EXPECT_TRUE(result.empty());
}

TEST(Snapshot, MaxEntitiesCapped) {
    // Create more than MAX_ENTITIES_PER_PACKET entities
    std::vector<EntitySnapshot> entities;
    for (uint32_t i = 0; i < MAX_ENTITIES_PER_PACKET + 10; ++i) {
        EntitySnapshot e;
        e.entity_id = i;
        entities.push_back(e);
    }

    auto packet = serialize_snapshot(entities, 0, 0, 0);
    auto result = deserialize_snapshot(packet.data(), packet.size());
    EXPECT_EQ(result.size(), MAX_ENTITIES_PER_PACKET);
}

// =============================================================================
// LAN broadcast serialize/deserialize
// =============================================================================

TEST(LANBroadcast, RoundTrip) {
    LANBroadcastPayload original;
    std::strncpy(original.server_name, "Test Server", sizeof(original.server_name) - 1);
    original.player_count = 3;
    original.max_players = 8;
    original.port = 7777;
    std::strncpy(original.map_name, "arena_01", sizeof(original.map_name) - 1);

    auto packet = serialize_lan_broadcast(original);
    EXPECT_LE(packet.size(), MAX_PACKET_SIZE);

    auto header = deserialize_header(packet.data(), packet.size());
    EXPECT_EQ(header.type, PacketType::LAN_BROADCAST);

    auto result = deserialize_lan_broadcast(packet.data(), packet.size());
    EXPECT_STREQ(result.server_name, "Test Server");
    EXPECT_EQ(result.player_count, 3);
    EXPECT_EQ(result.max_players, 8);
    EXPECT_EQ(result.port, 7777);
    EXPECT_STREQ(result.map_name, "arena_01");
}

TEST(LANBroadcast, EmptyFields) {
    LANBroadcastPayload original;
    original.player_count = 0;
    original.max_players = 16;
    original.port = 9999;

    auto packet = serialize_lan_broadcast(original);
    auto result = deserialize_lan_broadcast(packet.data(), packet.size());
    EXPECT_STREQ(result.server_name, "");
    EXPECT_EQ(result.player_count, 0);
    EXPECT_EQ(result.max_players, 16);
    EXPECT_EQ(result.port, 9999);
}

// =============================================================================
// Delta compression
// =============================================================================

TEST(DeltaCompression, DetectsPositionChange) {
    std::vector<EntitySnapshot> old_snap = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};
    std::vector<EntitySnapshot> new_snap = {{
        1, vec3{10, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};

    auto deltas = compute_snapshot_delta(old_snap, new_snap);
    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_TRUE(deltas[0].position_changed);
    EXPECT_FALSE(deltas[0].rotation_changed);
    EXPECT_FALSE(deltas[0].health_changed);
    EXPECT_FALSE(deltas[0].state_changed);
    EXPECT_FLOAT_EQ(deltas[0].position.x, 10.0f);
}

TEST(DeltaCompression, DetectsRotationChange) {
    std::vector<EntitySnapshot> old_snap = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};
    quat new_rot = glm::normalize(quat{0.7f, 0.7f, 0.0f, 0.0f});
    std::vector<EntitySnapshot> new_snap = {{
        1, vec3{0, 0, 0}, new_rot, 100.0f, 5.0f, 0, 0
    }};

    auto deltas = compute_snapshot_delta(old_snap, new_snap);
    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_FALSE(deltas[0].position_changed);
    EXPECT_TRUE(deltas[0].rotation_changed);
}

TEST(DeltaCompression, DetectsHealthChange) {
    std::vector<EntitySnapshot> old_snap = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};
    std::vector<EntitySnapshot> new_snap = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 50.0f, 5.0f, 0, 0
    }};

    auto deltas = compute_snapshot_delta(old_snap, new_snap);
    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_TRUE(deltas[0].health_changed);
    EXPECT_FLOAT_EQ(deltas[0].health, 50.0f);
}

TEST(DeltaCompression, DetectsStateChange) {
    std::vector<EntitySnapshot> old_snap = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};
    std::vector<EntitySnapshot> new_snap = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 2, 0
    }};

    auto deltas = compute_snapshot_delta(old_snap, new_snap);
    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_TRUE(deltas[0].state_changed);
    EXPECT_EQ(deltas[0].state, 2);
}

TEST(DeltaCompression, FiltersNoChange) {
    std::vector<EntitySnapshot> snap = {{
        1, vec3{5, 10, 15}, quat{1, 0, 0, 0}, 80.0f, 5.0f, 1, 0
    }};

    auto deltas = compute_snapshot_delta(snap, snap);
    EXPECT_TRUE(deltas.empty());
}

TEST(DeltaCompression, NewEntityFullDelta) {
    std::vector<EntitySnapshot> old_snap; // empty
    std::vector<EntitySnapshot> new_snap = {{
        42, vec3{1, 2, 3}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};

    auto deltas = compute_snapshot_delta(old_snap, new_snap);
    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_EQ(deltas[0].entity_id, 42u);
    EXPECT_TRUE(deltas[0].position_changed);
    EXPECT_TRUE(deltas[0].rotation_changed);
    EXPECT_TRUE(deltas[0].health_changed);
    EXPECT_TRUE(deltas[0].state_changed);
}

TEST(DeltaCompression, SmallChangeBelowThreshold) {
    std::vector<EntitySnapshot> old_snap = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};
    // Position change smaller than default threshold (0.01)
    std::vector<EntitySnapshot> new_snap = {{
        1, vec3{0.001f, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};

    auto deltas = compute_snapshot_delta(old_snap, new_snap);
    EXPECT_TRUE(deltas.empty());
}

// =============================================================================
// Delta apply
// =============================================================================

TEST(DeltaApply, ApplyPositionChange) {
    std::vector<EntitySnapshot> base = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};

    EntityDelta delta;
    delta.entity_id = 1;
    delta.position_changed = true;
    delta.position = vec3{10, 20, 30};

    auto result = apply_snapshot_delta(base, {delta});
    ASSERT_EQ(result.size(), 1u);
    EXPECT_FLOAT_EQ(result[0].position.x, 10.0f);
    EXPECT_FLOAT_EQ(result[0].position.y, 20.0f);
    EXPECT_FLOAT_EQ(result[0].position.z, 30.0f);
    // Unchanged fields
    EXPECT_FLOAT_EQ(result[0].health, 100.0f);
}

TEST(DeltaApply, AddNewEntity) {
    std::vector<EntitySnapshot> base = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};

    EntityDelta delta;
    delta.entity_id = 2;
    delta.position_changed = true;
    delta.position = vec3{5, 5, 5};
    delta.health_changed = true;
    delta.health = 50.0f;

    auto result = apply_snapshot_delta(base, {delta});
    ASSERT_EQ(result.size(), 2u);
    // Original entity unchanged
    EXPECT_EQ(result[0].entity_id, 1u);
    // New entity added
    EXPECT_EQ(result[1].entity_id, 2u);
    EXPECT_FLOAT_EQ(result[1].position.x, 5.0f);
    EXPECT_FLOAT_EQ(result[1].health, 50.0f);
}

TEST(DeltaApply, RoundTripDeltaComputeApply) {
    std::vector<EntitySnapshot> old_snap = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }, {
        2, vec3{10, 10, 10}, quat{1, 0, 0, 0}, 80.0f, 3.0f, 1, 1
    }};

    std::vector<EntitySnapshot> new_snap = {{
        1, vec3{5, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }, {
        2, vec3{10, 10, 10}, quat{1, 0, 0, 0}, 60.0f, 3.0f, 2, 1
    }};

    auto deltas = compute_snapshot_delta(old_snap, new_snap);
    auto reconstructed = apply_snapshot_delta(old_snap, deltas);

    ASSERT_EQ(reconstructed.size(), 2u);
    // Entity 1: position changed
    EXPECT_FLOAT_EQ(reconstructed[0].position.x, 5.0f);
    EXPECT_FLOAT_EQ(reconstructed[0].health, 100.0f);
    // Entity 2: health and state changed
    EXPECT_FLOAT_EQ(reconstructed[1].health, 60.0f);
    EXPECT_EQ(reconstructed[1].state, 2);
}

// =============================================================================
// Delta serialization round-trip
// =============================================================================

TEST(DeltaSerialization, RoundTrip) {
    std::vector<EntityDelta> deltas;

    EntityDelta d1;
    d1.entity_id = 1;
    d1.position_changed = true;
    d1.position = vec3{1, 2, 3};
    d1.rotation_changed = false;
    d1.health_changed = true;
    d1.health = 50.0f;
    d1.state_changed = false;
    deltas.push_back(d1);

    EntityDelta d2;
    d2.entity_id = 2;
    d2.position_changed = false;
    d2.rotation_changed = true;
    d2.rotation = glm::normalize(quat{0.5f, 0.5f, 0.5f, 0.5f});
    d2.health_changed = false;
    d2.state_changed = true;
    d2.state = 3;
    deltas.push_back(d2);

    auto packet = serialize_delta_snapshot(deltas, 10, 8, 0xFF);
    EXPECT_LE(packet.size(), MAX_PACKET_SIZE);

    auto header = deserialize_header(packet.data(), packet.size());
    EXPECT_EQ(header.type, PacketType::DELTA_SNAPSHOT);

    auto result = deserialize_delta_snapshot(packet.data(), packet.size());
    ASSERT_EQ(result.size(), 2u);

    EXPECT_EQ(result[0].entity_id, 1u);
    EXPECT_TRUE(result[0].position_changed);
    EXPECT_FALSE(result[0].rotation_changed);
    EXPECT_TRUE(result[0].health_changed);
    EXPECT_FALSE(result[0].state_changed);
    EXPECT_FLOAT_EQ(result[0].position.x, 1.0f);
    EXPECT_FLOAT_EQ(result[0].health, 50.0f);

    EXPECT_EQ(result[1].entity_id, 2u);
    EXPECT_FALSE(result[1].position_changed);
    EXPECT_TRUE(result[1].rotation_changed);
    EXPECT_FALSE(result[1].health_changed);
    EXPECT_TRUE(result[1].state_changed);
    EXPECT_EQ(result[1].state, 3);
}

// =============================================================================
// Snapshot interpolation
// =============================================================================

TEST(SnapshotInterpolation, T0ReturnsFrom) {
    std::vector<EntitySnapshot> from = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};
    std::vector<EntitySnapshot> to = {{
        1, vec3{10, 20, 30}, quat{1, 0, 0, 0}, 50.0f, 10.0f, 1, 1
    }};

    auto result = interpolate_snapshots(from, to, 0.0f);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_FLOAT_EQ(result[0].position.x, 0.0f);
    EXPECT_FLOAT_EQ(result[0].position.y, 0.0f);
    EXPECT_FLOAT_EQ(result[0].position.z, 0.0f);
    EXPECT_FLOAT_EQ(result[0].health, 100.0f);
    EXPECT_EQ(result[0].state, 0); // discrete: from at t<0.5
}

TEST(SnapshotInterpolation, T1ReturnsTo) {
    std::vector<EntitySnapshot> from = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};
    std::vector<EntitySnapshot> to = {{
        1, vec3{10, 20, 30}, quat{1, 0, 0, 0}, 50.0f, 10.0f, 1, 1
    }};

    auto result = interpolate_snapshots(from, to, 1.0f);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_FLOAT_EQ(result[0].position.x, 10.0f);
    EXPECT_FLOAT_EQ(result[0].position.y, 20.0f);
    EXPECT_FLOAT_EQ(result[0].position.z, 30.0f);
    EXPECT_FLOAT_EQ(result[0].health, 50.0f);
    EXPECT_EQ(result[0].state, 1); // discrete: to at t>=0.5
}

TEST(SnapshotInterpolation, T05IsMidpoint) {
    std::vector<EntitySnapshot> from = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 0.0f, 0, 0
    }};
    std::vector<EntitySnapshot> to = {{
        1, vec3{10, 20, 30}, quat{1, 0, 0, 0}, 50.0f, 10.0f, 1, 1
    }};

    auto result = interpolate_snapshots(from, to, 0.5f);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_NEAR(result[0].position.x, 5.0f, 1e-4f);
    EXPECT_NEAR(result[0].position.y, 10.0f, 1e-4f);
    EXPECT_NEAR(result[0].position.z, 15.0f, 1e-4f);
    EXPECT_NEAR(result[0].health, 75.0f, 1e-4f);
    EXPECT_NEAR(result[0].speed, 5.0f, 1e-4f);
    // At exactly t=0.5, state comes from "to"
    EXPECT_EQ(result[0].state, 1);
}

TEST(SnapshotInterpolation, T025) {
    std::vector<EntitySnapshot> from = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 0.0f, 0, 0
    }};
    std::vector<EntitySnapshot> to = {{
        1, vec3{100, 0, 0}, quat{1, 0, 0, 0}, 0.0f, 20.0f, 3, 0
    }};

    auto result = interpolate_snapshots(from, to, 0.25f);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_NEAR(result[0].position.x, 25.0f, 1e-4f);
    EXPECT_NEAR(result[0].health, 75.0f, 1e-4f);
    EXPECT_NEAR(result[0].speed, 5.0f, 1e-4f);
    EXPECT_EQ(result[0].state, 0); // t < 0.5 => from
}

TEST(SnapshotInterpolation, NewEntityInTo) {
    std::vector<EntitySnapshot> from = {{
        1, vec3{0, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }};
    std::vector<EntitySnapshot> to = {{
        1, vec3{10, 0, 0}, quat{1, 0, 0, 0}, 100.0f, 5.0f, 0, 0
    }, {
        2, vec3{50, 50, 50}, quat{1, 0, 0, 0}, 80.0f, 3.0f, 0, 0
    }};

    auto result = interpolate_snapshots(from, to, 0.5f);
    ASSERT_EQ(result.size(), 2u);
    // Entity 2 should be present (new spawn)
    bool found_entity_2 = false;
    for (const auto& e : result) {
        if (e.entity_id == 2) {
            found_entity_2 = true;
            EXPECT_FLOAT_EQ(e.position.x, 50.0f);
        }
    }
    EXPECT_TRUE(found_entity_2);
}

// =============================================================================
// Protocol constants
// =============================================================================

TEST(ProtocolConstants, ProtocolIdIsODYS) {
    EXPECT_EQ(PROTOCOL_ID, 0x4F445953u);
}

TEST(ProtocolConstants, MaxPacketSizeIsMTUSafe) {
    EXPECT_LE(MAX_PACKET_SIZE, 1500u); // below Ethernet MTU
}

TEST(ProtocolConstants, HeaderSize) {
    PacketWriter writer;
    PacketHeader header;
    writer.write_header(header);
    EXPECT_EQ(writer.size(), 16u); // 4+2+2+4+1+3 = 16
}
