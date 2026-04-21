#pragma once
#include "net/socket.h"
#include "net/protocol.h"
#include "core/types.h"
#include "core/result.h"
#include <functional>
#include <string>
#include <vector>
#include <deque>

namespace odyssey::audio::voice { struct VoiceFrame; }

namespace odyssey::net {

// Callback invoked when a VOICE_FRAME relayed by the server arrives on this
// client. The audio layer subscribes via set_voice_frame_callback(). Passed by
// const-ref so the callback can copy the opus_payload into a jitter buffer
// without a second allocation.
using VoiceFrameCallback =
    std::function<void(const odyssey::audio::voice::VoiceFrame& frame)>;

enum class ClientState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

struct ClientConfig {
    std::string player_name = "Player";
    float timeout = 10.0f;
    float input_send_rate = 60.0f;  // inputs per second
};

class Client {
public:
    Result<bool> connect(const Address& server_addr, const ClientConfig& config);
    void disconnect();

    // Process network events (call each frame)
    void tick(float delta_time);

    // Send player input
    void send_input(const InputPayload& input);

    // Send one Opus-encoded voice frame upstream to the server. `opus_payload`
    // is opaque bytes from the Codec; the client layer owns the per-speaker
    // u16 sequence and flag assembly. If size would exceed MAX_PACKET_SIZE
    // after headers, the frame is dropped (voice is never fragmented).
    // Returns true on send, false on drop/not-connected.
    bool send_voice_frame(const uint8_t* opus_payload,
                          size_t size,
                          uint16_t sequence,
                          uint8_t flags);

    // Subscribe to inbound voice frames. Replaces any prior callback.
    void set_voice_frame_callback(VoiceFrameCallback cb) { voice_cb_ = std::move(cb); }

    // Get latest server snapshot
    const std::vector<EntitySnapshot>& get_snapshot() const { return current_snapshot_; }

    // State
    ClientState get_state() const { return state_; }
    bool is_connected() const { return state_ == ClientState::CONNECTED; }

    // Stats
    float get_rtt() const { return rtt_; }
    float get_packet_loss() const { return packet_loss_; }

private:
    void process_packet(const uint8_t* data, size_t size);
    void handle_connect_accept();
    void handle_snapshot(const std::vector<EntitySnapshot>& snapshot, const PacketHeader& header);
    void send_heartbeat();

    UDPSocket socket_;
    Address server_address_;
    ClientConfig config_;
    ClientState state_ = ClientState::DISCONNECTED;

    std::vector<EntitySnapshot> current_snapshot_;

    // Prediction
    struct PendingInput {
        InputPayload input;
        uint32_t sequence;
    };
    std::deque<PendingInput> pending_inputs_;

    // Connection tracking
    uint16_t sequence_ = 0;
    uint16_t remote_sequence_ = 0;
    uint32_t ack_bits_ = 0;
    float time_since_last_recv_ = 0.0f;
    float input_send_timer_ = 0.0f;
    float heartbeat_timer_ = 0.0f;

    // Stats
    float rtt_ = 0.0f;
    float packet_loss_ = 0.0f;

    // Voice
    VoiceFrameCallback voice_cb_;
};

} // namespace odyssey::net
