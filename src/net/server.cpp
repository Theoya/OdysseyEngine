#include "net/server.h"
#include "audio/voice/voice_frame.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace odyssey::net {

Result<bool> Server::start(const ServerConfig& config) {
    if (running_) {
        return Result<bool>::err("Server already running");
    }

    config_ = config;

    // Open main game socket
    SocketConfig sock_cfg;
    sock_cfg.port = config.port;
    sock_cfg.non_blocking = true;
    auto result = socket_.open(sock_cfg);
    if (result.is_err()) {
        return Result<bool>::err("Failed to open server socket: " + result.error());
    }

    // Open broadcast socket for LAN discovery
    if (config.lan_broadcast) {
        SocketConfig bcast_cfg;
        bcast_cfg.port = 0; // ephemeral port for sending broadcasts
        bcast_cfg.broadcast = true;
        bcast_cfg.non_blocking = true;
        auto bcast_result = broadcast_socket_.open(bcast_cfg);
        if (bcast_result.is_err()) {
            spdlog::warn("Failed to open broadcast socket: {}", bcast_result.error());
            // Non-fatal: server can still run without LAN discovery
        }
    }

    // Initialize client slots
    clients_.resize(config.max_players);
    for (auto& slot : clients_) {
        slot.connected = false;
        slot.player_entity = INVALID_ENTITY;
    }

    running_ = true;
    sequence_ = 0;
    broadcast_timer_ = 0.0f;
    snapshot_timer_ = 0.0f;

    spdlog::info("Server '{}' started on port {} (max {} players, map: {})",
                 config.server_name, config.port, config.max_players, config.map_name);
    return Result<bool>::ok(true);
}

void Server::stop() {
    if (!running_) return;

    // Send disconnect to all connected clients
    PacketWriter writer;
    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.sequence = sequence_++;
    header.type = PacketType::DISCONNECT;
    writer.write_header(header);

    for (auto& slot : clients_) {
        if (slot.connected) {
            socket_.send_to(slot.address, writer.data(), writer.size());
            slot.connected = false;
        }
    }

    socket_.close();
    broadcast_socket_.close();
    clients_.clear();
    current_states_.clear();
    running_ = false;

    spdlog::info("Server stopped");
}

void Server::tick(float delta_time) {
    if (!running_) return;

    // Receive and process packets
    uint8_t buffer[MAX_PACKET_SIZE];
    Address sender;

    for (int i = 0; i < 256; ++i) { // process up to 256 packets per tick
        auto result = socket_.recv_from(sender, buffer, sizeof(buffer));
        if (result.is_err()) break;
        int bytes = result.value();
        if (bytes <= 0) break;

        process_packet(sender, buffer, static_cast<size_t>(bytes));
    }

    // Send snapshots at snapshot_rate
    snapshot_timer_ += delta_time;
    float snapshot_interval = 1.0f / config_.snapshot_rate;
    if (snapshot_timer_ >= snapshot_interval) {
        snapshot_timer_ -= snapshot_interval;
        send_snapshots();
    }

    // LAN broadcast
    if (config_.lan_broadcast && broadcast_socket_.is_open()) {
        broadcast_timer_ += delta_time;
        if (broadcast_timer_ >= 1.0f) { // broadcast every second
            broadcast_timer_ -= 1.0f;
            send_lan_broadcast();
        }
    }

    // Check for client timeouts
    check_timeouts(delta_time);
}

void Server::set_entity_states(const std::vector<EntitySnapshot>& states) {
    current_states_ = states;
}

uint8_t Server::player_count() const {
    uint8_t count = 0;
    for (const auto& slot : clients_) {
        if (slot.connected) ++count;
    }
    return count;
}

void Server::process_packet(const Address& sender, const uint8_t* data, size_t size) {
    if (size < 16) return; // minimum header size

    auto header = deserialize_header(data, size);
    if (header.protocol_id != PROTOCOL_ID) {
        spdlog::debug("Rejected packet with invalid protocol ID from {}", sender.to_string());
        return;
    }

    switch (header.type) {
        case PacketType::CONNECT_REQUEST: {
            auto payload = deserialize_connect(data, size);
            handle_connect(sender, payload);
            break;
        }
        case PacketType::INPUT: {
            auto input = deserialize_input(data, size);
            handle_input(sender, input, header);
            break;
        }
        case PacketType::DISCONNECT: {
            handle_disconnect(sender);
            break;
        }
        case PacketType::HEARTBEAT: {
            auto* client = find_client(sender);
            if (client) {
                client->time_since_last_packet = 0.0f;
            }
            break;
        }
        case PacketType::PING: {
            // Respond with PONG
            PacketWriter writer;
            PacketHeader pong;
            pong.protocol_id = PROTOCOL_ID;
            pong.sequence = sequence_++;
            pong.type = PacketType::PONG;
            writer.write_header(pong);
            socket_.send_to(sender, writer.data(), writer.size());
            break;
        }
        case PacketType::VOICE_FRAME: {
            // Voice relay is its own code path — extracted for unit testing
            // (tests call handle_voice_frame directly with a capturing send fn).
            handle_voice_frame(sender, data, size);
            break;
        }
        default:
            break;
    }
}

void Server::voice_send(const Address& dst, const uint8_t* data, size_t size) {
    if (voice_send_fn_) {
        voice_send_fn_(dst, data, size);
    } else {
        socket_.send_to(dst, data, size);
    }
}

void Server::set_client_position(size_t slot_index, const vec3& pos) {
    if (slot_index < clients_.size()) clients_[slot_index].voice.position = pos;
}

void Server::set_client_voice_range(size_t slot_index, float range_m) {
    if (slot_index < clients_.size()) {
        // Hard clamp on write, not just on relay. Defence in depth: even if a
        // bug somewhere sets the value above the cap, the ingress path can't
        // trust the field anyway. Council: "netcode — anti-cheat hard-clamp".
        clients_[slot_index].voice.voice_range =
            std::min(std::max(range_m, 0.0f), VOICE_RANGE_MAX_METERS);
    }
}

void Server::handle_voice_frame(const Address& sender,
                                const uint8_t* data,
                                size_t size) {
    // 1. AUTHED SENDER LOOKUP. Unknown senders are dropped — a v2 VOICE_FRAME
    //    from a non-connected address is either stray junk or a probe; either
    //    way, don't relay. This also prevents amplification attacks (attacker
    //    spoofing source IP to make the server spam a victim).
    ClientSlot* sender_slot = find_client(sender);
    if (!sender_slot || !sender_slot->connected) {
        spdlog::debug("Voice frame from non-connected {} — dropped", sender.to_string());
        return;
    }

    // 2. PARSE + VALIDATE. deserialize_voice_frame enforces protocol id,
    //    packet type, size ceiling, and reserved-flag bits. Any failure =
    //    silent drop (these are unreliable frames; no NACK).
    auto parsed = odyssey::audio::voice::deserialize_voice_frame(data, size);
    if (parsed.is_err()) {
        spdlog::debug("Voice frame from {} failed parse: {}",
                      sender.to_string(),
                      odyssey::audio::voice::to_string(parsed.error()));
        return;
    }
    auto vf = std::move(parsed).value();

    // 3. REPLAY / STALE-SEQUENCE GUARD. Drop frames more than 32 units old;
    //    wrap-aware. Matches design doc §8.
    const uint16_t seq = vf.voice.sequence;
    if (sender_slot->voice.has_voice_sequence) {
        const int16_t delta =
            static_cast<int16_t>(seq - sender_slot->voice.last_voice_sequence);
        if (delta <= 0 && delta > -32) {
            // Late or duplicate — drop but don't advance the window.
            return;
        }
        if (delta <= -32) {
            // Way too old: replay or a very confused peer. Drop.
            return;
        }
    }
    sender_slot->voice.last_voice_sequence = seq;
    sender_slot->voice.has_voice_sequence = true;

    // 4. SILENCE SUPPRESSION. Zero-length payload would encode to nothing
    //    useful, and END_OF_TALK is a local-client hint (jitter flush) that
    //    could be delivered via a lightweight marker — but per design doc we
    //    explicitly DO NOT relay these, to save bandwidth. Listeners use a
    //    silence-timeout disarm instead. Council condition "silence
    //    suppression" + "END_OF_TALK not relayed" in the test matrix.
    const bool end_of_talk =
        (vf.voice.flags & odyssey::net::voice_flags::END_OF_TALK) != 0;
    if (vf.opus_payload.empty() || end_of_talk) {
        return;
    }

    // 5. ANTI-SPOOF REBIND. The CLIENT's speaker_entity_id field is ignored;
    //    we overwrite with the authed sender's player_entity. This is the
    //    keystone of the voice-impersonation mitigation. NEVER TRUST THE
    //    CLIENT-SUPPLIED speaker_entity_id.
    vf.voice.speaker_entity_id = sender_slot->player_entity;

    // 6. CLAMP voice_range on ingress. A compromised client cannot widen its
    //    own voice_range stat to broadcast globally. Hard cap = 50 m.
    const float clamped_range = std::min(
        std::max(sender_slot->voice.voice_range, 0.0f),
        VOICE_RANGE_MAX_METERS);

    // 7. RE-SERIALIZE with the rewritten speaker_entity_id. The Opus payload
    //    bytes are reused verbatim — no transcode, no repack. This preserves
    //    the end-to-end "bits the speaker encoded are the bits the listener
    //    decodes" invariant.
    const auto relay_bytes = odyssey::audio::voice::serialize_voice_frame(vf);

    // 8. FAN OUT with per-listener interest filter.
    //    d = ||speaker.pos - listener.pos||. Listener is in range iff d <= R.
    const vec3 speaker_pos = sender_slot->voice.position;
    for (auto& listener : clients_) {
        if (!listener.connected) continue;
        if (&listener == sender_slot) continue; // never echo to self

        const vec3 d = listener.voice.position - speaker_pos;
        const float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
        const float r2 = clamped_range * clamped_range;
        if (dist2 > r2) continue; // out of range

        voice_send(listener.address, relay_bytes.data(), relay_bytes.size());
    }
}

void Server::handle_connect(const Address& sender, const ConnectPayload& payload) {
    // Check version
    if (payload.version != PROTOCOL_VERSION) {
        PacketWriter writer;
        PacketHeader header;
        header.protocol_id = PROTOCOL_ID;
        header.sequence = sequence_++;
        header.type = PacketType::CONNECT_REJECT;
        writer.write_header(header);
        socket_.send_to(sender, writer.data(), writer.size());
        spdlog::info("Rejected connection from {} (version mismatch)", sender.to_string());
        return;
    }

    // Check if already connected
    auto* existing = find_client(sender);
    if (existing && existing->connected) {
        // Re-send accept
        PacketWriter writer;
        PacketHeader header;
        header.protocol_id = PROTOCOL_ID;
        header.sequence = sequence_++;
        header.type = PacketType::CONNECT_ACCEPT;
        writer.write_header(header);
        socket_.send_to(sender, writer.data(), writer.size());
        return;
    }

    // Find free slot
    auto* slot = find_free_slot();
    if (!slot) {
        PacketWriter writer;
        PacketHeader header;
        header.protocol_id = PROTOCOL_ID;
        header.sequence = sequence_++;
        header.type = PacketType::CONNECT_REJECT;
        writer.write_header(header);
        socket_.send_to(sender, writer.data(), writer.size());
        spdlog::info("Rejected connection from {} (server full)", sender.to_string());
        return;
    }

    // Accept connection
    slot->address = sender;
    slot->name = payload.player_name;
    slot->connected = true;
    slot->time_since_last_packet = 0.0f;
    slot->last_sequence = 0;
    slot->last_ack = 0;
    slot->ack_bits = 0;

    PacketWriter writer;
    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.sequence = sequence_++;
    header.type = PacketType::CONNECT_ACCEPT;
    writer.write_header(header);
    socket_.send_to(sender, writer.data(), writer.size());

    spdlog::info("Client '{}' connected from {} ({}/{} players)",
                 payload.player_name, sender.to_string(),
                 player_count(), config_.max_players);
}

void Server::handle_input(const Address& sender, const InputPayload& input, const PacketHeader& header) {
    auto* client = find_client(sender);
    if (!client || !client->connected) return;

    client->time_since_last_packet = 0.0f;
    client->last_input = input;
    client->last_sequence = header.sequence;
    client->last_ack = header.ack;
    client->ack_bits = header.ack_bits;
}

void Server::handle_disconnect(const Address& sender) {
    auto* client = find_client(sender);
    if (!client) return;

    spdlog::info("Client '{}' disconnected from {}", client->name, sender.to_string());
    client->connected = false;
    client->player_entity = INVALID_ENTITY;
    client->name.clear();
}

void Server::send_snapshots() {
    if (current_states_.empty()) return;

    auto packet = serialize_snapshot(current_states_, sequence_++, 0, 0);

    for (auto& slot : clients_) {
        if (!slot.connected) continue;
        socket_.send_to(slot.address, packet.data(), packet.size());
    }
}

void Server::send_lan_broadcast() {
    LANBroadcastPayload payload;
    std::strncpy(payload.server_name, config_.server_name.c_str(),
                 sizeof(payload.server_name) - 1);
    payload.player_count = player_count();
    payload.max_players = config_.max_players;
    payload.port = config_.port;
    std::strncpy(payload.map_name, config_.map_name.c_str(),
                 sizeof(payload.map_name) - 1);

    auto packet = serialize_lan_broadcast(payload);
    broadcast_socket_.broadcast(config_.broadcast_port, packet.data(), packet.size());
}

void Server::check_timeouts(float dt) {
    constexpr float CLIENT_TIMEOUT = 15.0f;

    for (auto& slot : clients_) {
        if (!slot.connected) continue;

        slot.time_since_last_packet += dt;
        if (slot.time_since_last_packet >= CLIENT_TIMEOUT) {
            spdlog::info("Client '{}' timed out from {}", slot.name, slot.address.to_string());
            slot.connected = false;
            slot.player_entity = INVALID_ENTITY;
            slot.name.clear();
        }
    }
}

ClientSlot* Server::find_client(const Address& addr) {
    for (auto& slot : clients_) {
        if (slot.connected && slot.address == addr) {
            return &slot;
        }
    }
    return nullptr;
}

ClientSlot* Server::find_free_slot() {
    for (auto& slot : clients_) {
        if (!slot.connected) {
            return &slot;
        }
    }
    return nullptr;
}

} // namespace odyssey::net
