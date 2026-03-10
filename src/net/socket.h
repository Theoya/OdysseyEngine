#pragma once
#include "core/result.h"
#include <string>
#include <vector>
#include <cstdint>
#include <array>

namespace odyssey::net {

struct Address {
    std::string host = "0.0.0.0";
    uint16_t port = 0;

    bool operator==(const Address& other) const;
    bool operator!=(const Address& other) const;
    std::string to_string() const;
};

// Pure: compute socket config
struct SocketConfig {
    uint16_t port = 0;
    bool broadcast = false;      // enable broadcast for LAN discovery
    bool non_blocking = true;
    int recv_buffer_size = 65536;
    int send_buffer_size = 65536;
};

class UDPSocket {
public:
    UDPSocket() = default;
    ~UDPSocket();

    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    UDPSocket(UDPSocket&& other) noexcept;
    UDPSocket& operator=(UDPSocket&& other) noexcept;

    // Impure: open/close
    Result<bool> open(const SocketConfig& config);
    void close();
    bool is_open() const;

    // Impure: send/recv
    Result<int> send_to(const Address& dest, const void* data, size_t size);
    Result<int> recv_from(Address& sender, void* buffer, size_t buffer_size);

    // Impure: broadcast (for LAN discovery)
    Result<int> broadcast(uint16_t port, const void* data, size_t size);

    uint16_t local_port() const;

private:
    // Use intptr_t to avoid including platform headers
    intptr_t socket_handle_ = -1;
    uint16_t port_ = 0;
};

// Initialize/shutdown networking (Winsock on Windows)
Result<bool> init_networking();
void shutdown_networking();

} // namespace odyssey::net
