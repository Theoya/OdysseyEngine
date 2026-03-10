#pragma once
#include "net/socket.h"
#include "net/protocol.h"
#include "core/result.h"
#include <vector>
#include <string>
#include <chrono>

namespace odyssey::net {

struct ServerInfo {
    Address address;
    std::string server_name;
    std::string map_name;
    uint8_t player_count = 0;
    uint8_t max_players = 8;
    float time_since_seen = 0.0f; // for timeout
};

class LANScanner {
public:
    // Start scanning for LAN servers
    Result<bool> start(uint16_t listen_port = 7776);
    void stop();

    // Poll for new responses (call periodically)
    void tick(float delta_time);

    // Send a broadcast probe
    void send_probe(uint16_t target_port = 7776);

    // Get discovered servers
    const std::vector<ServerInfo>& get_servers() const { return servers_; }

    // Clear old servers
    void prune_stale(float timeout = 10.0f);

    bool is_scanning() const { return scanning_; }

private:
    UDPSocket socket_;
    std::vector<ServerInfo> servers_;
    bool scanning_ = false;
    float probe_timer_ = 0.0f;
};

} // namespace odyssey::net
