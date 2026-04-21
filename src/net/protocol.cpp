#include "net/protocol.h"
#include <algorithm>
#include <cstring>

namespace odyssey::net {

// ─── PacketWriter ─────────────────────────────────────────────────────────────

void PacketWriter::write_u8(uint8_t v) {
    buffer_.push_back(v);
}

void PacketWriter::write_u16(uint16_t v) {
    buffer_.push_back(static_cast<uint8_t>(v & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void PacketWriter::write_u32(uint32_t v) {
    buffer_.push_back(static_cast<uint8_t>(v & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void PacketWriter::write_float(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(float));
    write_u32(bits);
}

void PacketWriter::write_vec3(vec3 v) {
    write_float(v.x);
    write_float(v.y);
    write_float(v.z);
}

void PacketWriter::write_quat(quat q) {
    write_float(q.w);
    write_float(q.x);
    write_float(q.y);
    write_float(q.z);
}

void PacketWriter::write_string(const char* str, size_t max_len) {
    size_t len = std::min(std::strlen(str), max_len - 1); // reserve 1 byte for null
    size_t old_size = buffer_.size();
    buffer_.resize(old_size + max_len, 0);
    std::memcpy(buffer_.data() + old_size, str, len);
}

void PacketWriter::write_bytes(const void* data, size_t size) {
    size_t old_size = buffer_.size();
    buffer_.resize(old_size + size);
    std::memcpy(buffer_.data() + old_size, data, size);
}

void PacketWriter::write_header(const PacketHeader& header) {
    write_u32(header.protocol_id);
    write_u16(header.sequence);
    write_u16(header.ack);
    write_u32(header.ack_bits);
    write_u8(static_cast<uint8_t>(header.type));
    write_u8(header.padding[0]);
    write_u8(header.padding[1]);
    write_u8(header.padding[2]);
}

// ─── PacketReader ─────────────────────────────────────────────────────────────

uint8_t PacketReader::read_u8() {
    if (offset_ >= size_) return 0;
    return data_[offset_++];
}

uint16_t PacketReader::read_u16() {
    uint16_t v = 0;
    v |= static_cast<uint16_t>(read_u8());
    v |= static_cast<uint16_t>(read_u8()) << 8;
    return v;
}

uint32_t PacketReader::read_u32() {
    uint32_t v = 0;
    v |= static_cast<uint32_t>(read_u8());
    v |= static_cast<uint32_t>(read_u8()) << 8;
    v |= static_cast<uint32_t>(read_u8()) << 16;
    v |= static_cast<uint32_t>(read_u8()) << 24;
    return v;
}

float PacketReader::read_float() {
    uint32_t bits = read_u32();
    float v;
    std::memcpy(&v, &bits, sizeof(float));
    return v;
}

vec3 PacketReader::read_vec3() {
    float x = read_float();
    float y = read_float();
    float z = read_float();
    return vec3{x, y, z};
}

quat PacketReader::read_quat() {
    float w = read_float();
    float x = read_float();
    float y = read_float();
    float z = read_float();
    return quat{w, x, y, z};
}

void PacketReader::read_string(char* out, size_t max_len) {
    size_t bytes_to_read = std::min(max_len, size_ - offset_);
    std::memcpy(out, data_ + offset_, bytes_to_read);
    offset_ += max_len; // always advance by max_len (fixed-size field)
}

void PacketReader::read_bytes(void* out, size_t size) {
    size_t bytes_to_read = std::min(size, size_ - offset_);
    std::memcpy(out, data_ + offset_, bytes_to_read);
    offset_ += size;
}

PacketHeader PacketReader::read_header() {
    PacketHeader header;
    header.protocol_id = read_u32();
    header.sequence = read_u16();
    header.ack = read_u16();
    header.ack_bits = read_u32();
    header.type = static_cast<PacketType>(read_u8());
    header.padding[0] = read_u8();
    header.padding[1] = read_u8();
    header.padding[2] = read_u8();
    return header;
}

// ─── Serialize functions ──────────────────────────────────────────────────────

std::vector<uint8_t> serialize_connect_request(const ConnectPayload& payload, uint16_t sequence) {
    PacketWriter writer;

    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.sequence = sequence;
    header.type = PacketType::CONNECT_REQUEST;
    writer.write_header(header);

    writer.write_string(payload.player_name, 32);
    writer.write_u32(payload.version);

    return std::vector<uint8_t>(writer.data(), writer.data() + writer.size());
}

std::vector<uint8_t> serialize_input(const InputPayload& input, uint16_t sequence, uint16_t ack, uint32_t ack_bits) {
    PacketWriter writer;

    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.sequence = sequence;
    header.ack = ack;
    header.ack_bits = ack_bits;
    header.type = PacketType::INPUT;
    writer.write_header(header);

    writer.write_float(input.forward);
    writer.write_float(input.strafe);
    writer.write_float(input.look_yaw);
    writer.write_float(input.look_pitch);
    writer.write_u8(input.buttons);
    writer.write_u32(input.sequence);

    return std::vector<uint8_t>(writer.data(), writer.data() + writer.size());
}

std::vector<uint8_t> serialize_snapshot(const std::vector<EntitySnapshot>& entities, uint16_t sequence, uint16_t ack, uint32_t ack_bits) {
    PacketWriter writer;

    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.sequence = sequence;
    header.ack = ack;
    header.ack_bits = ack_bits;
    header.type = PacketType::SNAPSHOT;
    writer.write_header(header);

    uint32_t count = static_cast<uint32_t>(std::min(entities.size(),
                                                     MAX_ENTITIES_PER_PACKET));
    writer.write_u32(count);

    for (uint32_t i = 0; i < count; ++i) {
        const auto& e = entities[i];
        writer.write_u32(e.entity_id);
        writer.write_vec3(e.position);
        writer.write_quat(e.rotation);
        writer.write_float(e.health);
        writer.write_float(e.speed);
        writer.write_u8(e.state);
        writer.write_u8(e.flags);
    }

    return std::vector<uint8_t>(writer.data(), writer.data() + writer.size());
}

std::vector<uint8_t> serialize_lan_broadcast(const LANBroadcastPayload& payload) {
    PacketWriter writer;

    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.type = PacketType::LAN_BROADCAST;
    writer.write_header(header);

    writer.write_string(payload.server_name, 32);
    writer.write_u8(payload.player_count);
    writer.write_u8(payload.max_players);
    writer.write_u16(payload.port);
    writer.write_string(payload.map_name, 32);

    return std::vector<uint8_t>(writer.data(), writer.data() + writer.size());
}

// ─── Deserialize functions ────────────────────────────────────────────────────

PacketHeader deserialize_header(const uint8_t* data, size_t size) {
    PacketReader reader(data, size);
    return reader.read_header();
}

ConnectPayload deserialize_connect(const uint8_t* data, size_t size) {
    PacketReader reader(data, size);
    reader.read_header(); // skip header

    ConnectPayload payload;
    reader.read_string(payload.player_name, 32);
    payload.version = reader.read_u32();
    return payload;
}

InputPayload deserialize_input(const uint8_t* data, size_t size) {
    PacketReader reader(data, size);
    reader.read_header(); // skip header

    InputPayload input;
    input.forward = reader.read_float();
    input.strafe = reader.read_float();
    input.look_yaw = reader.read_float();
    input.look_pitch = reader.read_float();
    input.buttons = reader.read_u8();
    input.sequence = reader.read_u32();
    return input;
}

std::vector<EntitySnapshot> deserialize_snapshot(const uint8_t* data, size_t size) {
    PacketReader reader(data, size);
    reader.read_header(); // skip header

    uint32_t count = reader.read_u32();
    std::vector<EntitySnapshot> entities;
    entities.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        EntitySnapshot e;
        e.entity_id = reader.read_u32();
        e.position = reader.read_vec3();
        e.rotation = reader.read_quat();
        e.health = reader.read_float();
        e.speed = reader.read_float();
        e.state = reader.read_u8();
        e.flags = reader.read_u8();
        entities.push_back(e);
    }

    return entities;
}

LANBroadcastPayload deserialize_lan_broadcast(const uint8_t* data, size_t size) {
    PacketReader reader(data, size);
    reader.read_header(); // skip header

    LANBroadcastPayload payload;
    reader.read_string(payload.server_name, 32);
    payload.player_count = reader.read_u8();
    payload.max_players = reader.read_u8();
    payload.port = reader.read_u16();
    reader.read_string(payload.map_name, 32);
    return payload;
}

// ─── Voice sub-header ─────────────────────────────────────────────────────────
// Wire order matches the byte-layout comment in protocol.h. LE u32 / LE u16
// use the same encoding as every other field in the packet, which keeps the
// whole packet uniformly little-endian and debugger-friendly with a single
// hexdump mental model.

void write_voice_subheader(PacketWriter& w, const VoiceSubHeader& sh) {
    w.write_u32(sh.speaker_entity_id); // offset 0..3
    w.write_u16(sh.sequence);          // offset 4..5
    w.write_u8(sh.frame_ms);           // offset 6
    w.write_u8(sh.flags);              // offset 7
}

VoiceSubHeader read_voice_subheader(PacketReader& r) {
    VoiceSubHeader sh;
    sh.speaker_entity_id = r.read_u32();
    sh.sequence          = r.read_u16();
    sh.frame_ms          = r.read_u8();
    sh.flags             = r.read_u8();
    return sh;
}

} // namespace odyssey::net
