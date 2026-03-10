#pragma once
#include "core/types.h"
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

namespace odyssey::net {

// Protocol constants
constexpr uint32_t PROTOCOL_ID = 0x4F445953; // "ODYS"
constexpr uint32_t PROTOCOL_VERSION = 1;
constexpr size_t MAX_PACKET_SIZE = 1200; // MTU-safe
constexpr size_t MAX_ENTITIES_PER_PACKET = 32;

// Packet types
enum class PacketType : uint8_t {
    // Connection
    CONNECT_REQUEST = 1,
    CONNECT_ACCEPT = 2,
    CONNECT_REJECT = 3,
    DISCONNECT = 4,
    HEARTBEAT = 5,

    // Game state
    INPUT = 10,           // client -> server: player input
    SNAPSHOT = 11,        // server -> client: world state snapshot
    DELTA_SNAPSHOT = 12,  // server -> client: delta-compressed state

    // LAN discovery
    LAN_BROADCAST = 20,  // server -> broadcast: "I exist"
    LAN_RESPONSE = 21,   // server -> client: server info

    // Misc
    PING = 30,
    PONG = 31,
};

// Packet header (16 bytes)
struct PacketHeader {
    uint32_t protocol_id = PROTOCOL_ID;
    uint16_t sequence = 0;      // packet sequence number
    uint16_t ack = 0;           // last received sequence from peer
    uint32_t ack_bits = 0;      // bitfield of received packets
    PacketType type = PacketType::HEARTBEAT;
    uint8_t padding[3] = {};
};

// Input packet payload
struct InputPayload {
    float forward = 0;     // -1 to 1
    float strafe = 0;      // -1 to 1
    float look_yaw = 0;    // radians
    float look_pitch = 0;  // radians
    uint8_t buttons = 0;   // bit flags: shoot, jump, reload, interact, etc.
    uint32_t sequence = 0; // input sequence for reconciliation
};

// Entity state for snapshot
struct EntitySnapshot {
    EntityID entity_id = INVALID_ENTITY;
    vec3 position{0.f};
    quat rotation{1, 0, 0, 0};
    float health = 0;
    float speed = 0;
    uint8_t state = 0;     // behavior state
    uint8_t flags = 0;     // active, visible, etc.
};

// Connect request
struct ConnectPayload {
    char player_name[32] = {};
    uint32_t version = PROTOCOL_VERSION;
};

// LAN broadcast
struct LANBroadcastPayload {
    char server_name[32] = {};
    uint8_t player_count = 0;
    uint8_t max_players = 8;
    uint16_t port = 0;
    char map_name[32] = {};
};

// ===== SERIALIZATION (Pure functions) =====

// Buffer writer for packet construction
class PacketWriter {
public:
    PacketWriter() { buffer_.reserve(MAX_PACKET_SIZE); }

    void write_u8(uint8_t v);
    void write_u16(uint16_t v);
    void write_u32(uint32_t v);
    void write_float(float v);
    void write_vec3(vec3 v);
    void write_quat(quat q);
    void write_string(const char* str, size_t max_len);
    void write_bytes(const void* data, size_t size);
    void write_header(const PacketHeader& header);

    const uint8_t* data() const { return buffer_.data(); }
    size_t size() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
};

// Buffer reader for packet parsing
class PacketReader {
public:
    PacketReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    uint8_t read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    float read_float();
    vec3 read_vec3();
    quat read_quat();
    void read_string(char* out, size_t max_len);
    void read_bytes(void* out, size_t size);
    PacketHeader read_header();

    size_t remaining() const { return size_ - offset_; }
    bool has_data() const { return offset_ < size_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t offset_ = 0;
};

// Pure: serialize full packets
std::vector<uint8_t> serialize_connect_request(const ConnectPayload& payload, uint16_t sequence);
std::vector<uint8_t> serialize_input(const InputPayload& input, uint16_t sequence, uint16_t ack, uint32_t ack_bits);
std::vector<uint8_t> serialize_snapshot(const std::vector<EntitySnapshot>& entities, uint16_t sequence, uint16_t ack, uint32_t ack_bits);
std::vector<uint8_t> serialize_lan_broadcast(const LANBroadcastPayload& payload);

// Pure: deserialize
PacketHeader deserialize_header(const uint8_t* data, size_t size);
ConnectPayload deserialize_connect(const uint8_t* data, size_t size);
InputPayload deserialize_input(const uint8_t* data, size_t size);
std::vector<EntitySnapshot> deserialize_snapshot(const uint8_t* data, size_t size);
LANBroadcastPayload deserialize_lan_broadcast(const uint8_t* data, size_t size);

} // namespace odyssey::net
