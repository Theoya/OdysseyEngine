#pragma once
#include "core/types.h"
#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

namespace odyssey::net {

// Protocol constants
constexpr uint32_t PROTOCOL_ID = 0x4F445953; // "ODYS"
// PROTOCOL_VERSION bumped 1 -> 2 on 2026-04-20 to introduce VOICE_FRAME and
// the 8-byte voice sub-header. See docs/decisions/2026-04-20-proximity-voice-chat.md
// and docs/design/proximity_chat_netcode.md for the ratified design. The bump
// is a hard gate: v1 clients connecting to a v2 server receive CONNECT_REJECT
// (see test_protocol_version_compat.cpp) — there is no silent downgrade, because
// the wire now contains packet types a v1 peer cannot parse.
constexpr uint32_t PROTOCOL_VERSION = 2;
constexpr size_t MAX_PACKET_SIZE = 1200; // MTU-safe
constexpr size_t MAX_ENTITIES_PER_PACKET = 32;

// Server-enforced ceiling on the per-speaker voice_range stat. A compromised
// client cannot broadcast map-wide by authoring a 10000m voice_range on its
// prefab — the server hard-clamps on ingress. See council condition
// "netcode — anti-cheat hard-clamp" in the ratified decision record.
constexpr float VOICE_RANGE_MAX_METERS = 50.0f;

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

    // Proximity voice (v2+). Server relays VOICE_FRAME from speaker to
    // in-range listeners after rebinding speaker_entity_id to the authed
    // sender (anti-spoof). Opus payload is byte-exact pass-through.
    VOICE_FRAME   = 40,
    VOICE_CONTROL = 41,  // reserved: PTT state, mute toggles, etc.
};

// ─── Voice sub-header (v2) ────────────────────────────────────────────────────
// Layout after the 16-byte PacketHeader when type == VOICE_FRAME:
//
//   offset  size  field
//      0      4   speaker_entity_id   (u32, LE). Server REWRITES on relay to
//                                     the authed sender's controlled entity.
//                                     Clients SHOULD send 0; the server never
//                                     trusts this field on ingress.
//      4      2   sequence            (u16, LE). Per-speaker voice sequence,
//                                     wraps. Separate from PacketHeader.sequence
//                                     because voice cadence (50 Hz) differs
//                                     from game tick and from reliable acks.
//      6      1   frame_ms            (u8). 20 today. Reserved 10/40/60.
//      7      1   flags               (u8). Bit layout:
//                                       bit0 VAD_ACTIVE
//                                       bit1 PTT_HELD
//                                       bit2 FEC_PRESENT      (reserved)
//                                       bit3 END_OF_TALK
//                                       bit4-7 reserved, MUST be 0.
//      8      N   opus_payload        Opus frame bytes. 24 kbps CBR × 20 ms
//                                     ≈ 60 B typical (RFC 6716 §2.1.1).
//
// Total per voice packet on the wire:
//   20 IP + 8 UDP + 16 PacketHeader + 8 voice sub-header + ~60 Opus ≈ 112 B.
// Capped at MAX_PACKET_SIZE (1200 B) incl. header; voice frames are NEVER
// fragmented — if Opus output would exceed the ceiling the frame is dropped
// at the encoder (design doc §5).
struct VoiceSubHeader {
    uint32_t speaker_entity_id = 0;
    uint16_t sequence = 0;
    uint8_t  frame_ms = 20;
    uint8_t  flags = 0;
};
static_assert(sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) == 8,
              "VoiceSubHeader wire size must be exactly 8 bytes");

// Voice flag bits. Kept out of an enum class so they can be OR'd with uint8_t
// without casting noise in call sites.
namespace voice_flags {
constexpr uint8_t VAD_ACTIVE   = 1u << 0;
constexpr uint8_t PTT_HELD     = 1u << 1;
constexpr uint8_t FEC_PRESENT  = 1u << 2; // reserved, in-band Opus FEC later
constexpr uint8_t END_OF_TALK  = 1u << 3;
constexpr uint8_t RESERVED_MASK = 0b11110000u; // bits 4-7 MUST be zero in v2
} // namespace voice_flags

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

// ─── Voice sub-header wire helpers (pure) ─────────────────────────────────────
// Encode/decode operate ONLY on the 8-byte voice sub-header, not the Opus
// payload bytes. The caller is responsible for appending/consuming the
// opaque opus_payload that follows.
void write_voice_subheader(PacketWriter& w, const VoiceSubHeader& sh);
VoiceSubHeader read_voice_subheader(PacketReader& r);

} // namespace odyssey::net
