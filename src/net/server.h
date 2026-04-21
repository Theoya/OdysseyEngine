#pragma once
#include "net/socket.h"
#include "net/protocol.h"
#include "core/types.h"
#include "core/result.h"
#include <functional>
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

// Minimal voice-side state attached to a client slot. Kept flat here so voice
// can ride the existing connection without a parallel channel table.
struct VoiceClientState {
    uint16_t last_voice_sequence = 0;  // for stale/replay detection server-side
    bool has_voice_sequence = false;   // false until first VOICE_FRAME seen
    float voice_range = 20.0f;         // metres; clamped to VOICE_RANGE_MAX_METERS
    vec3  position = vec3(0.0f);       // cached for interest filter (per tick)
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
    VoiceClientState voice;
};

// Send-function seam for testing voice_relay without a real socket. Tests
// pass a capturing lambda to set_voice_send_fn() so the relay decision can
// be observed in-memory without spinning up a UDP loopback.
using VoiceRelaySendFn = std::function<void(const Address& dst,
                                            const uint8_t* data,
                                            size_t size)>;

class Server {
public:
    Result<bool> start(const ServerConfig& config);
    void stop();

    // Process network events (call each server tick)
    void tick(float delta_time);

    // Get current client inputs (for applying to game simulation)
    const std::vector<ClientSlot>& get_clients() const { return clients_; }
    std::vector<ClientSlot>& get_clients_mutable() { return clients_; }

    // Set entity states for next snapshot
    void set_entity_states(const std::vector<EntitySnapshot>& states);

    // Check if running
    bool is_running() const { return running_; }

    // Stats
    uint8_t player_count() const;

    // ─── Voice relay path ────────────────────────────────────────────────────
    // Called by process_packet() on VOICE_FRAME. Exposed public for unit tests
    // (test_voice_relay.cpp) — the real socket is swapped out via
    // set_voice_send_fn() to capture relayed packets in memory.
    //
    // `sender` is the raw UDP source; server authenticates by matching to a
    // connected ClientSlot, then REWRITES the speaker_entity_id in the frame
    // before relay (anti-spoof). A missing / disconnected sender is dropped.
    void handle_voice_frame(const Address& sender,
                            const uint8_t* data,
                            size_t size);

    // Test seam: redirect voice send() calls to a functor instead of the
    // real UDPSocket. Passing nullptr restores the real socket.
    void set_voice_send_fn(VoiceRelaySendFn fn) { voice_send_fn_ = std::move(fn); }

    // Test seam: set a client's cached position (what the interest filter
    // reads). In production this is updated from the simulation via
    // set_entity_states() — tests shortcut straight to this.
    void set_client_position(size_t slot_index, const vec3& pos);
    void set_client_voice_range(size_t slot_index, float range_m);

private:
    void process_packet(const Address& sender, const uint8_t* data, size_t size);
    void handle_connect(const Address& sender, const ConnectPayload& payload);
    void handle_input(const Address& sender, const InputPayload& input, const PacketHeader& header);
    void handle_disconnect(const Address& sender);
    void send_snapshots();
    void send_lan_broadcast();
    void check_timeouts(float dt);

    // Unified send helper for voice — either the real socket or the test fn.
    void voice_send(const Address& dst, const uint8_t* data, size_t size);

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

    VoiceRelaySendFn voice_send_fn_; // null => use socket_
};

} // namespace odyssey::net
