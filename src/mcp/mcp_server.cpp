#include "mcp/mcp_server.h"
#include "mcp/json_helpers.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <sstream>

// ---------------------------------------------------------------------------
// Platform socket abstraction
// ---------------------------------------------------------------------------

#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <WinSock2.h>
#   include <WS2tcpip.h>
#   pragma comment(lib, "ws2_32.lib")

    using socket_t = SOCKET;
    static constexpr socket_t INVALID_SOCK = INVALID_SOCKET;

    namespace {
    struct WinsockInit {
        WinsockInit() {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
        }
        ~WinsockInit() { WSACleanup(); }
    };
    // Ensure Winsock is initialized once.
    static WinsockInit& ensure_winsock() {
        static WinsockInit instance;
        return instance;
    }
    void close_socket(socket_t s) { closesocket(s); }
    } // anonymous namespace

#else
#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <sys/socket.h>
#   include <unistd.h>

    using socket_t = int;
    static constexpr socket_t INVALID_SOCK = -1;

    namespace {
    void close_socket(socket_t s) { close(s); }
    } // anonymous namespace
#endif

namespace odyssey::mcp {

// ---------------------------------------------------------------------------
// JsonRpcResponse::to_json
// ---------------------------------------------------------------------------

std::string JsonRpcResponse::to_json() const {
    json::JsonBuilder b;
    b.begin_object()
        .key("jsonrpc").value("2.0")
        .key("id").value(id);

    if (success) {
        b.key("result").raw(result.empty() ? "null" : result);
    } else {
        b.key("error").begin_object()
            .key("code").value(static_cast<int64_t>(-32603))
            .key("message").value(error)
        .end_object();
    }

    b.end_object();
    return b.build();
}

// ---------------------------------------------------------------------------
// MCPServer lifetime
// ---------------------------------------------------------------------------

MCPServer::~MCPServer() {
    if (running_.load()) {
        stop();
    }
}

// ---------------------------------------------------------------------------
// register_tool
// ---------------------------------------------------------------------------

void MCPServer::register_tool(const ToolDefinition& tool) {
    std::lock_guard<std::mutex> lock(tools_mutex_);
    tools_[tool.name] = tool;
    spdlog::info("MCP: registered tool '{}'", tool.name);
}

// ---------------------------------------------------------------------------
// start
// ---------------------------------------------------------------------------

Result<bool> MCPServer::start(uint16_t port) {
    if (running_.load()) {
        return Result<bool>::err("MCP server is already running");
    }

#ifdef _WIN32
    ensure_winsock();
#endif

    port_ = port;

    // Create TCP socket
    auto sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCK) {
        return Result<bool>::err("Failed to create TCP socket");
    }

    // Allow address reuse
    int opt = 1;
#ifdef _WIN32
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only
    addr.sin_port = htons(port_);

    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(sock);
        return Result<bool>::err("Failed to bind to port " + std::to_string(port_));
    }

    // Listen
    if (::listen(sock, 4) != 0) {
        close_socket(sock);
        return Result<bool>::err("Failed to listen on port " + std::to_string(port_));
    }

    server_socket_ = static_cast<intptr_t>(sock);
    running_.store(true);

    server_thread_ = std::thread(&MCPServer::server_loop, this);

    spdlog::info("MCP server listening on 127.0.0.1:{}", port_);
    return Result<bool>::ok(true);
}

// ---------------------------------------------------------------------------
// stop
// ---------------------------------------------------------------------------

void MCPServer::stop() {
    running_.store(false);

    // Close the server socket to unblock accept()
    if (server_socket_ != -1) {
        close_socket(static_cast<socket_t>(server_socket_));
        server_socket_ = -1;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    spdlog::info("MCP server stopped");
}

// ---------------------------------------------------------------------------
// server_loop  (runs on background thread)
// ---------------------------------------------------------------------------

void MCPServer::server_loop() {
    spdlog::info("MCP server thread started");

    while (running_.load()) {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif

        auto client = ::accept(static_cast<socket_t>(server_socket_),
                               reinterpret_cast<sockaddr*>(&client_addr),
                               &addr_len);

        if (!running_.load()) break;

        if (client == INVALID_SOCK) {
            if (running_.load()) {
                spdlog::warn("MCP: accept() failed");
            }
            continue;
        }

        spdlog::debug("MCP: client connected");

        // Handle synchronously on this thread (simple single-client model).
        // For concurrent clients a thread pool could be used.
        handle_client(static_cast<intptr_t>(client));
    }

    spdlog::info("MCP server thread exiting");
}

// ---------------------------------------------------------------------------
// handle_client
// ---------------------------------------------------------------------------

void MCPServer::handle_client(intptr_t client_socket) {
    auto sock = static_cast<socket_t>(client_socket);

    // Read newline-delimited JSON-RPC messages.
    std::string buffer;
    char chunk[4096];

    while (running_.load()) {
        // Receive data
#ifdef _WIN32
        int n = ::recv(sock, chunk, sizeof(chunk) - 1, 0);
#else
        ssize_t n = ::recv(sock, chunk, sizeof(chunk) - 1, 0);
#endif
        if (n <= 0) break; // client disconnected or error

        chunk[n] = '\0';
        buffer += chunk;

        // Process complete lines (newline-delimited JSON-RPC)
        size_t newline_pos;
        while ((newline_pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, newline_pos);
            buffer.erase(0, newline_pos + 1);

            // Trim trailing \r if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Skip empty lines
            if (line.empty()) continue;

            // Parse and dispatch
            auto parse_result = parse_request(line);
            JsonRpcResponse response;

            if (parse_result.is_err()) {
                response.success = false;
                response.error = parse_result.error();
                response.id = "null";
            } else {
                auto& request = parse_result.value();
                response = dispatch(request);
                response.id = request.id;
            }

            // Send response (newline-delimited)
            std::string response_str = response.to_json() + "\n";
#ifdef _WIN32
            ::send(sock, response_str.c_str(),
                   static_cast<int>(response_str.size()), 0);
#else
            ::send(sock, response_str.c_str(), response_str.size(), 0);
#endif
        }
    }

    close_socket(sock);
    spdlog::debug("MCP: client disconnected");
}

// ---------------------------------------------------------------------------
// dispatch
// ---------------------------------------------------------------------------

JsonRpcResponse MCPServer::dispatch(const JsonRpcRequest& request) {
    // Special built-in method: list tools
    if (request.method == "tools/list" || request.method == "mcp.list_tools") {
        return handle_list_tools();
    }

    std::lock_guard<std::mutex> lock(tools_mutex_);

    auto it = tools_.find(request.method);
    if (it == tools_.end()) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Method not found: " + request.method;
        return resp;
    }

    try {
        return it->second.handler(request);
    } catch (const std::exception& e) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = std::string("Internal error: ") + e.what();
        return resp;
    }
}

// ---------------------------------------------------------------------------
// handle_list_tools
// ---------------------------------------------------------------------------

JsonRpcResponse MCPServer::handle_list_tools() {
    std::lock_guard<std::mutex> lock(tools_mutex_);

    json::JsonBuilder b;
    b.begin_object().key("tools").begin_array();

    for (const auto& [name, tool] : tools_) {
        b.begin_object()
            .key("name").value(name)
            .key("description").value(tool.description)
            .key("inputSchema").raw(
                tool.input_schema.empty() ? "{}" : tool.input_schema)
        .end_object();
    }

    b.end_array().end_object();

    JsonRpcResponse resp;
    resp.success = true;
    resp.result = b.build();
    return resp;
}

// ---------------------------------------------------------------------------
// parse_request
// ---------------------------------------------------------------------------

Result<JsonRpcRequest> MCPServer::parse_request(const std::string& raw) {
    // Minimal JSON-RPC parsing.
    // Expected format: {"jsonrpc":"2.0","id":"...","method":"...","params":{...}}

    auto method = json::get_string(raw, "method");
    if (!method.has_value()) {
        return Result<JsonRpcRequest>::err("Missing 'method' field");
    }

    JsonRpcRequest req;
    req.method = std::move(*method);

    // ID can be a string or number. Try string first, fall back to number.
    auto id_str = json::get_string(raw, "id");
    if (id_str.has_value()) {
        req.id = std::move(*id_str);
    } else {
        auto id_num = json::get_int(raw, "id");
        if (id_num.has_value()) {
            req.id = std::to_string(*id_num);
        } else {
            req.id = "null";
        }
    }

    // Extract params as a raw JSON substring.
    // Find "params" key and extract the object/value after it.
    // For simplicity, store everything from the params value start.
    auto params_str = json::get_string(raw, "params");
    if (params_str.has_value()) {
        // params was a string — unusual but handle it
        req.params = std::move(*params_str);
    } else {
        // Try to extract the raw object. Find "params" : { ... }
        std::string key_pattern = "\"params\"";
        size_t key_pos = raw.find(key_pattern);
        if (key_pos != std::string::npos) {
            size_t colon = raw.find(':', key_pos + key_pattern.size());
            if (colon != std::string::npos) {
                // Find the start of the value
                size_t val_start = colon + 1;
                while (val_start < raw.size() &&
                       std::isspace(static_cast<unsigned char>(raw[val_start]))) {
                    ++val_start;
                }

                if (val_start < raw.size() && raw[val_start] == '{') {
                    // Find matching closing brace (simple brace counting)
                    int depth = 0;
                    bool in_string = false;
                    size_t i = val_start;
                    for (; i < raw.size(); ++i) {
                        char c = raw[i];
                        if (in_string) {
                            if (c == '\\') { ++i; continue; }
                            if (c == '"') in_string = false;
                        } else {
                            if (c == '"') in_string = true;
                            else if (c == '{') ++depth;
                            else if (c == '}') {
                                --depth;
                                if (depth == 0) { ++i; break; }
                            }
                        }
                    }
                    req.params = raw.substr(val_start, i - val_start);
                }
            }
        }
    }

    return Result<JsonRpcRequest>::ok(std::move(req));
}

} // namespace odyssey::mcp
