#include "net/lobby.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>

namespace odyssey::net {

Result<bool> LANScanner::start(uint16_t listen_port) {
    if (scanning_) {
        return Result<bool>::err("Already scanning");
    }

    SocketConfig config;
    config.port = listen_port;
    config.broadcast = true;
    config.non_blocking = true;
    auto result = socket_.open(config);
    if (result.is_err()) {
        return Result<bool>::err("Failed to open scanner socket: " + result.error());
    }

    scanning_ = true;
    probe_timer_ = 0.0f;
    servers_.clear();

    spdlog::info("LAN scanner started on port {}", listen_port);
    return Result<bool>::ok(true);
}

void LANScanner::stop() {
    if (!scanning_) return;

    socket_.close();
    scanning_ = false;
    servers_.clear();

    spdlog::info("LAN scanner stopped");
}

void LANScanner::tick(float delta_time) {
    if (!scanning_) return;

    // Update time_since_seen for all servers
    for (auto& server : servers_) {
        server.time_since_seen += delta_time;
    }

    // Periodic probe
    probe_timer_ += delta_time;
    if (probe_timer_ >= 2.0f) { // probe every 2 seconds
        probe_timer_ -= 2.0f;
        send_probe(socket_.local_port());
    }

    // Receive LAN broadcast responses
    uint8_t buffer[MAX_PACKET_SIZE];
    Address sender;

    for (int i = 0; i < 32; ++i) {
        auto result = socket_.recv_from(sender, buffer, sizeof(buffer));
        if (result.is_err()) break;
        int bytes = result.value();
        if (bytes <= 0) break;

        // Parse packet
        if (static_cast<size_t>(bytes) < 16) continue;
        auto header = deserialize_header(buffer, static_cast<size_t>(bytes));
        if (header.protocol_id != PROTOCOL_ID) continue;
        if (header.type != PacketType::LAN_BROADCAST) continue;

        auto payload = deserialize_lan_broadcast(buffer, static_cast<size_t>(bytes));

        // Update or add server info
        bool found = false;
        for (auto& server : servers_) {
            if (server.address.host == sender.host && server.address.port == payload.port) {
                server.server_name = payload.server_name;
                server.map_name = payload.map_name;
                server.player_count = payload.player_count;
                server.max_players = payload.max_players;
                server.time_since_seen = 0.0f;
                found = true;
                break;
            }
        }

        if (!found) {
            ServerInfo info;
            info.address.host = sender.host;
            info.address.port = payload.port;
            info.server_name = payload.server_name;
            info.map_name = payload.map_name;
            info.player_count = payload.player_count;
            info.max_players = payload.max_players;
            info.time_since_seen = 0.0f;
            servers_.push_back(info);

            spdlog::info("Discovered LAN server '{}' at {}:{} ({}/{} players, map: {})",
                         info.server_name, info.address.host, info.address.port,
                         info.player_count, info.max_players, info.map_name);
        }
    }
}

void LANScanner::send_probe(uint16_t target_port) {
    if (!scanning_) return;

    // Send a LAN_BROADCAST packet as a probe
    // Servers listening on the broadcast port will respond with their info
    LANBroadcastPayload payload;
    std::strncpy(payload.server_name, "PROBE", sizeof(payload.server_name) - 1);
    payload.port = target_port;

    auto packet = serialize_lan_broadcast(payload);
    socket_.broadcast(target_port, packet.data(), packet.size());
}

void LANScanner::prune_stale(float timeout) {
    servers_.erase(
        std::remove_if(servers_.begin(), servers_.end(),
            [timeout](const ServerInfo& info) {
                return info.time_since_seen >= timeout;
            }),
        servers_.end()
    );
}

} // namespace odyssey::net
