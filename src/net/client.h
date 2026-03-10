#pragma once
#include "net/socket.h"
#include "net/protocol.h"
#include "core/types.h"
#include "core/result.h"
#include <string>
#include <vector>
#include <deque>

namespace odyssey::net {

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
};

} // namespace odyssey::net
