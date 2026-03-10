#pragma once
#include "net/socket.h"
#include "net/protocol.h"
#include "core/types.h"
#include "core/result.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace odyssey::net {

struct ServerConfig {
    uint16_t port = 7777;
    uint8_t max_players = 8;
    std::string server_name = "OdysseyEngine Server";
    std::string map_name = "default";
    float tick_rate = 60.0f;        // server ticks per second
    float snapshot_rate = 20.0f;    // snapshots per second to clients
    bool lan_broadcast = true;
    uint16_t broadcast_port = 7776;
};

struct ClientSlot {
    Address address;
    std::string name;
    EntityID player_entity = INVALID_ENTITY;
    uint16_t last_sequence = 0;
    uint16_t last_ack = 0;
    uint32_t ack_bits = 0;
    InputPayload last_input;
    float time_since_last_packet = 0.0f;
    bool connected = false;
};

class Server {
public:
    Result<bool> start(const ServerConfig& config);
    void stop();

    // Process network events (call each server tick)
    void tick(float delta_time);

    // Get current client inputs (for applying to game simulation)
    const std::vector<ClientSlot>& get_clients() const { return clients_; }

    // Set entity states for next snapshot
    void set_entity_states(const std::vector<EntitySnapshot>& states);

    // Check if running
    bool is_running() const { return running_; }

    // Stats
    uint8_t player_count() const;

private:
    void process_packet(const Address& sender, const uint8_t* data, size_t size);
    void handle_connect(const Address& sender, const ConnectPayload& payload);
    void handle_input(const Address& sender, const InputPayload& input, const PacketHeader& header);
    void handle_disconnect(const Address& sender);
    void send_snapshots();
    void send_lan_broadcast();
    void check_timeouts(float dt);

    ClientSlot* find_client(const Address& addr);
    ClientSlot* find_free_slot();

    UDPSocket socket_;
    UDPSocket broadcast_socket_; // for LAN discovery broadcasts
    ServerConfig config_;
    std::vector<ClientSlot> clients_;
    std::vector<EntitySnapshot> current_states_;

    uint16_t sequence_ = 0;
    float broadcast_timer_ = 0.0f;
    float snapshot_timer_ = 0.0f;
    bool running_ = false;
};

} // namespace odyssey::net
