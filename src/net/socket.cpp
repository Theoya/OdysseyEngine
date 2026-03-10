#include "net/socket.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <sstream>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socklen_t = int;
  #define SOCKET_INVALID static_cast<intptr_t>(INVALID_SOCKET)
  #define CLOSE_SOCKET(s) ::closesocket(static_cast<SOCKET>(s))
  #define LAST_ERROR WSAGetLastError()
  #define WOULD_BLOCK WSAEWOULDBLOCK
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #define SOCKET_INVALID static_cast<intptr_t>(-1)
  #define CLOSE_SOCKET(s) ::close(static_cast<int>(s))
  #define LAST_ERROR errno
  #define WOULD_BLOCK EWOULDBLOCK
#endif

namespace odyssey::net {

// ─── Address ──────────────────────────────────────────────────────────────────

bool Address::operator==(const Address& other) const {
    return host == other.host && port == other.port;
}

bool Address::operator!=(const Address& other) const {
    return !(*this == other);
}

std::string Address::to_string() const {
    std::ostringstream ss;
    ss << host << ":" << port;
    return ss.str();
}

// ─── Platform init/shutdown ───────────────────────────────────────────────────

Result<bool> init_networking() {
#ifdef _WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        spdlog::error("WSAStartup failed with error: {}", result);
        return Result<bool>::err("WSAStartup failed");
    }
    spdlog::info("Winsock initialized (version {}.{})",
                 LOBYTE(wsa_data.wVersion), HIBYTE(wsa_data.wVersion));
#endif
    return Result<bool>::ok(true);
}

void shutdown_networking() {
#ifdef _WIN32
    WSACleanup();
    spdlog::info("Winsock shut down");
#endif
}

// ─── UDPSocket ────────────────────────────────────────────────────────────────

UDPSocket::~UDPSocket() {
    close();
}

UDPSocket::UDPSocket(UDPSocket&& other) noexcept
    : socket_handle_(other.socket_handle_)
    , port_(other.port_) {
    other.socket_handle_ = -1;
    other.port_ = 0;
}

UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept {
    if (this != &other) {
        close();
        socket_handle_ = other.socket_handle_;
        port_ = other.port_;
        other.socket_handle_ = -1;
        other.port_ = 0;
    }
    return *this;
}

Result<bool> UDPSocket::open(const SocketConfig& config) {
    if (is_open()) {
        close();
    }

    // Create UDP socket
#ifdef _WIN32
    SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        spdlog::error("Failed to create UDP socket, error: {}", WSAGetLastError());
        return Result<bool>::err("Failed to create socket");
    }
    socket_handle_ = static_cast<intptr_t>(sock);
#else
    int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        spdlog::error("Failed to create UDP socket, error: {}", errno);
        return Result<bool>::err("Failed to create socket");
    }
    socket_handle_ = static_cast<intptr_t>(sock);
#endif

    // Set non-blocking
    if (config.non_blocking) {
#ifdef _WIN32
        u_long mode = 1;
        if (ioctlsocket(static_cast<SOCKET>(socket_handle_), FIONBIO, &mode) != 0) {
            spdlog::error("Failed to set non-blocking mode, error: {}", WSAGetLastError());
            close();
            return Result<bool>::err("Failed to set non-blocking");
        }
#else
        int flags = fcntl(static_cast<int>(socket_handle_), F_GETFL, 0);
        if (fcntl(static_cast<int>(socket_handle_), F_SETFL, flags | O_NONBLOCK) < 0) {
            spdlog::error("Failed to set non-blocking mode, error: {}", errno);
            close();
            return Result<bool>::err("Failed to set non-blocking");
        }
#endif
    }

    // Enable broadcast if requested
    if (config.broadcast) {
#ifdef _WIN32
        BOOL opt = TRUE;
        if (setsockopt(static_cast<SOCKET>(socket_handle_), SOL_SOCKET, SO_BROADCAST,
                       reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0) {
            spdlog::error("Failed to enable broadcast, error: {}", WSAGetLastError());
            close();
            return Result<bool>::err("Failed to enable broadcast");
        }
#else
        int opt = 1;
        if (setsockopt(static_cast<int>(socket_handle_), SOL_SOCKET, SO_BROADCAST,
                       &opt, sizeof(opt)) < 0) {
            spdlog::error("Failed to enable broadcast, error: {}", errno);
            close();
            return Result<bool>::err("Failed to enable broadcast");
        }
#endif
    }

    // Set buffer sizes
#ifdef _WIN32
    int recv_buf = config.recv_buffer_size;
    setsockopt(static_cast<SOCKET>(socket_handle_), SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<const char*>(&recv_buf), sizeof(recv_buf));
    int send_buf = config.send_buffer_size;
    setsockopt(static_cast<SOCKET>(socket_handle_), SOL_SOCKET, SO_SNDBUF,
               reinterpret_cast<const char*>(&send_buf), sizeof(send_buf));
#else
    int recv_buf = config.recv_buffer_size;
    setsockopt(static_cast<int>(socket_handle_), SOL_SOCKET, SO_RCVBUF,
               &recv_buf, sizeof(recv_buf));
    int send_buf = config.send_buffer_size;
    setsockopt(static_cast<int>(socket_handle_), SOL_SOCKET, SO_SNDBUF,
               &send_buf, sizeof(send_buf));
#endif

    // Allow address reuse
#ifdef _WIN32
    BOOL reuse = TRUE;
    setsockopt(static_cast<SOCKET>(socket_handle_), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    int reuse = 1;
    setsockopt(static_cast<int>(socket_handle_), SOL_SOCKET, SO_REUSEADDR,
               &reuse, sizeof(reuse));
#endif

    // Bind
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config.port);

#ifdef _WIN32
    if (::bind(static_cast<SOCKET>(socket_handle_),
               reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        spdlog::error("Failed to bind socket to port {}, error: {}", config.port, WSAGetLastError());
        close();
        return Result<bool>::err("Failed to bind socket");
    }
#else
    if (::bind(static_cast<int>(socket_handle_),
               reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::error("Failed to bind socket to port {}, error: {}", config.port, errno);
        close();
        return Result<bool>::err("Failed to bind socket");
    }
#endif

    // Retrieve the actual bound port (useful when config.port == 0)
    struct sockaddr_in bound_addr{};
    socklen_t bound_len = sizeof(bound_addr);
#ifdef _WIN32
    getsockname(static_cast<SOCKET>(socket_handle_),
                reinterpret_cast<struct sockaddr*>(&bound_addr), &bound_len);
#else
    getsockname(static_cast<int>(socket_handle_),
                reinterpret_cast<struct sockaddr*>(&bound_addr), &bound_len);
#endif
    port_ = ntohs(bound_addr.sin_port);

    spdlog::info("UDP socket opened on port {}", port_);
    return Result<bool>::ok(true);
}

void UDPSocket::close() {
    if (is_open()) {
        CLOSE_SOCKET(socket_handle_);
        spdlog::debug("UDP socket on port {} closed", port_);
        socket_handle_ = -1;
        port_ = 0;
    }
}

bool UDPSocket::is_open() const {
    return socket_handle_ != -1;
}

Result<int> UDPSocket::send_to(const Address& dest, const void* data, size_t size) {
    if (!is_open()) {
        return Result<int>::err("Socket not open");
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest.port);
    inet_pton(AF_INET, dest.host.c_str(), &addr.sin_addr);

#ifdef _WIN32
    int sent = ::sendto(static_cast<SOCKET>(socket_handle_),
                        static_cast<const char*>(data), static_cast<int>(size), 0,
                        reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (sent == SOCKET_ERROR) {
        return Result<int>::err("sendto failed: " + std::to_string(WSAGetLastError()));
    }
#else
    int sent = ::sendto(static_cast<int>(socket_handle_),
                        data, size, 0,
                        reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (sent < 0) {
        return Result<int>::err("sendto failed: " + std::to_string(errno));
    }
#endif

    return Result<int>::ok(sent);
}

Result<int> UDPSocket::recv_from(Address& sender, void* buffer, size_t buffer_size) {
    if (!is_open()) {
        return Result<int>::err("Socket not open");
    }

    struct sockaddr_in from_addr{};
    socklen_t from_len = sizeof(from_addr);

#ifdef _WIN32
    int received = ::recvfrom(static_cast<SOCKET>(socket_handle_),
                              static_cast<char*>(buffer), static_cast<int>(buffer_size), 0,
                              reinterpret_cast<struct sockaddr*>(&from_addr), &from_len);
    if (received == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            return Result<int>::ok(0); // no data available (non-blocking)
        }
        return Result<int>::err("recvfrom failed: " + std::to_string(err));
    }
#else
    int received = ::recvfrom(static_cast<int>(socket_handle_),
                              buffer, buffer_size, 0,
                              reinterpret_cast<struct sockaddr*>(&from_addr), &from_len);
    if (received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return Result<int>::ok(0); // no data available (non-blocking)
        }
        return Result<int>::err("recvfrom failed: " + std::to_string(errno));
    }
#endif

    // Fill sender address
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from_addr.sin_addr, ip_str, sizeof(ip_str));
    sender.host = ip_str;
    sender.port = ntohs(from_addr.sin_port);

    return Result<int>::ok(received);
}

Result<int> UDPSocket::broadcast(uint16_t port, const void* data, size_t size) {
    Address broadcast_addr;
    broadcast_addr.host = "255.255.255.255";
    broadcast_addr.port = port;
    return send_to(broadcast_addr, data, size);
}

uint16_t UDPSocket::local_port() const {
    return port_;
}

} // namespace odyssey::net
