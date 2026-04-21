#include "net/client.h"
#include "audio/voice/voice_frame.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace odyssey::net {

Result<bool> Client::connect(const Address& server_addr, const ClientConfig& config) {
    if (state_ != ClientState::DISCONNECTED) {
        return Result<bool>::err("Already connected or connecting");
    }

    config_ = config;
    server_address_ = server_addr;

    // Open client socket on ephemeral port
    SocketConfig sock_cfg;
    sock_cfg.port = 0; // OS-assigned port
    sock_cfg.non_blocking = true;
    auto result = socket_.open(sock_cfg);
    if (result.is_err()) {
        return Result<bool>::err("Failed to open client socket: " + result.error());
    }

    // Send connect request
    ConnectPayload payload;
    std::strncpy(payload.player_name, config.player_name.c_str(),
                 sizeof(payload.player_name) - 1);
    payload.version = PROTOCOL_VERSION;

    auto packet = serialize_connect_request(payload, sequence_++);
    auto send_result = socket_.send_to(server_address_, packet.data(), packet.size());
    if (send_result.is_err()) {
        socket_.close();
        return Result<bool>::err("Failed to send connect request: " + send_result.error());
    }

    state_ = ClientState::CONNECTING;
    time_since_last_recv_ = 0.0f;
    heartbeat_timer_ = 0.0f;
    input_send_timer_ = 0.0f;

    spdlog::info("Connecting to server at {}...", server_addr.to_string());
    return Result<bool>::ok(true);
}

void Client::disconnect() {
    if (state_ == ClientState::DISCONNECTED) return;

    // Send disconnect packet
    PacketWriter writer;
    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.sequence = sequence_++;
    header.type = PacketType::DISCONNECT;
    writer.write_header(header);
    socket_.send_to(server_address_, writer.data(), writer.size());

    socket_.close();
    state_ = ClientState::DISCONNECTED;
    current_snapshot_.clear();
    pending_inputs_.clear();
    rtt_ = 0.0f;
    packet_loss_ = 0.0f;

    spdlog::info("Disconnected from server");
}

void Client::tick(float delta_time) {
    if (state_ == ClientState::DISCONNECTED) return;

    // Receive packets
    uint8_t buffer[MAX_PACKET_SIZE];
    Address sender;

    for (int i = 0; i < 256; ++i) {
        auto result = socket_.recv_from(sender, buffer, sizeof(buffer));
        if (result.is_err()) break;
        int bytes = result.value();
        if (bytes <= 0) break;

        // Only process packets from our server
        if (sender == server_address_) {
            process_packet(buffer, static_cast<size_t>(bytes));
        }
    }

    // Update timers
    time_since_last_recv_ += delta_time;

    // Check timeout
    if (time_since_last_recv_ >= config_.timeout) {
        spdlog::warn("Connection to server timed out");
        state_ = ClientState::DISCONNECTED;
        socket_.close();
        return;
    }

    // Send heartbeats
    heartbeat_timer_ += delta_time;
    if (heartbeat_timer_ >= 1.0f) {
        heartbeat_timer_ -= 1.0f;
        send_heartbeat();

        // Resend connect request if still connecting
        if (state_ == ClientState::CONNECTING) {
            ConnectPayload payload;
            std::strncpy(payload.player_name, config_.player_name.c_str(),
                         sizeof(payload.player_name) - 1);
            payload.version = PROTOCOL_VERSION;
            auto packet = serialize_connect_request(payload, sequence_++);
            socket_.send_to(server_address_, packet.data(), packet.size());
        }
    }
}

void Client::send_input(const InputPayload& input) {
    if (state_ != ClientState::CONNECTED) return;

    auto packet = serialize_input(input, sequence_++, remote_sequence_, ack_bits_);
    socket_.send_to(server_address_, packet.data(), packet.size());

    // Store for prediction/reconciliation
    PendingInput pending;
    pending.input = input;
    pending.sequence = input.sequence;
    pending_inputs_.push_back(pending);

    // Limit pending input buffer size
    while (pending_inputs_.size() > 120) {
        pending_inputs_.pop_front();
    }
}

bool Client::send_voice_frame(const uint8_t* opus_payload,
                              size_t size,
                              uint16_t sequence,
                              uint8_t flags)
{
    if (state_ != ClientState::CONNECTED) return false;

    // Sanity: we never fragment voice. 16 B header + 8 B sub-header + payload
    // must fit in MAX_PACKET_SIZE. Drop (not split) on overflow.
    constexpr size_t kOverhead = 16 + 8;
    if (size > MAX_PACKET_SIZE - kOverhead) {
        spdlog::warn("Voice frame {} B exceeds MTU budget — dropped", size);
        return false;
    }

    // Assemble the VoiceFrame with speaker_entity_id = 0. The server rewrites
    // this field on relay (anti-spoof keystone, design doc §8). Clients are
    // documented to send 0; any other value is ignored by the server.
    odyssey::audio::voice::VoiceFrame vf;
    vf.header.protocol_id = PROTOCOL_ID;
    vf.header.sequence = sequence_++;
    vf.header.ack = remote_sequence_;
    vf.header.ack_bits = ack_bits_;
    vf.header.type = PacketType::VOICE_FRAME;
    vf.voice.speaker_entity_id = 0;      // server will rewrite
    vf.voice.sequence = sequence;
    vf.voice.frame_ms = 20;
    vf.voice.flags = flags;
    if (opus_payload && size > 0) {
        vf.opus_payload.assign(opus_payload, opus_payload + size);
    }

    auto bytes = odyssey::audio::voice::serialize_voice_frame(vf);
    auto r = socket_.send_to(server_address_, bytes.data(), bytes.size());
    return r.is_ok();
}

void Client::process_packet(const uint8_t* data, size_t size) {
    if (size < 16) return;

    auto header = deserialize_header(data, size);
    if (header.protocol_id != PROTOCOL_ID) return;

    time_since_last_recv_ = 0.0f;

    // Update ack tracking
    if (header.sequence > remote_sequence_ ||
        (remote_sequence_ > 0xFF00 && header.sequence < 0x00FF)) {
        // Shift ack bits
        uint16_t diff = header.sequence - remote_sequence_;
        if (diff <= 32) {
            ack_bits_ = (ack_bits_ << diff) | (1u << (diff - 1));
        } else {
            ack_bits_ = 0;
        }
        remote_sequence_ = header.sequence;
    } else {
        // Old or duplicate packet, still mark in ack bits
        uint16_t diff = remote_sequence_ - header.sequence;
        if (diff < 32) {
            ack_bits_ |= (1u << diff);
        }
    }

    switch (header.type) {
        case PacketType::CONNECT_ACCEPT:
            handle_connect_accept();
            break;

        case PacketType::CONNECT_REJECT:
            spdlog::warn("Connection rejected by server");
            state_ = ClientState::DISCONNECTED;
            socket_.close();
            break;

        case PacketType::DISCONNECT:
            spdlog::info("Server sent disconnect");
            state_ = ClientState::DISCONNECTED;
            socket_.close();
            break;

        case PacketType::SNAPSHOT: {
            auto snapshot = deserialize_snapshot(data, size);
            handle_snapshot(snapshot, header);
            break;
        }

        case PacketType::PONG:
            // RTT estimation could be done here by tracking ping send times
            break;

        case PacketType::VOICE_FRAME: {
            // Parse and dispatch to the audio layer callback. Dropping here
            // (no callback set) is normal for voice-disabled clients.
            if (!voice_cb_) break;
            auto parsed = odyssey::audio::voice::deserialize_voice_frame(data, size);
            if (parsed.is_err()) {
                spdlog::debug("Client voice frame parse failed: {}",
                              odyssey::audio::voice::to_string(parsed.error()));
                break;
            }
            voice_cb_(parsed.value());
            break;
        }

        default:
            break;
    }
}

void Client::handle_connect_accept() {
    if (state_ == ClientState::CONNECTING) {
        state_ = ClientState::CONNECTED;
        spdlog::info("Connected to server at {}", server_address_.to_string());
    }
}

void Client::handle_snapshot(const std::vector<EntitySnapshot>& snapshot, const PacketHeader& header) {
    current_snapshot_ = snapshot;

    // Reconciliation: remove acknowledged inputs
    // The server's ack tells us which of our input sequences have been processed
    while (!pending_inputs_.empty()) {
        // Remove inputs older than the ack'd sequence
        if (pending_inputs_.front().sequence <= header.ack) {
            pending_inputs_.pop_front();
        } else {
            break;
        }
    }
}

void Client::send_heartbeat() {
    PacketWriter writer;
    PacketHeader header;
    header.protocol_id = PROTOCOL_ID;
    header.sequence = sequence_++;
    header.ack = remote_sequence_;
    header.ack_bits = ack_bits_;
    header.type = PacketType::HEARTBEAT;
    writer.write_header(header);
    socket_.send_to(server_address_, writer.data(), writer.size());
}

} // namespace odyssey::net
