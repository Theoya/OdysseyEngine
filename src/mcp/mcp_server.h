#pragma once

#include "core/result.h"

#include <string>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>

namespace odyssey::mcp {

// JSON-RPC request/response (simple string-based for now)
struct JsonRpcRequest {
    std::string id;
    std::string method;
    std::string params;  // JSON string of parameters
};

struct JsonRpcResponse {
    std::string id;
    bool success = true;
    std::string result;  // JSON string result
    std::string error;   // error message if !success

    std::string to_json() const;
};

// Tool handler function type
using ToolHandler = std::function<JsonRpcResponse(const JsonRpcRequest&)>;

struct ToolDefinition {
    std::string name;
    std::string description;
    std::string input_schema;  // JSON schema string
    ToolHandler handler;
};

class MCPServer {
public:
    MCPServer() = default;
    ~MCPServer();

    MCPServer(const MCPServer&) = delete;
    MCPServer& operator=(const MCPServer&) = delete;

    // Register a tool
    void register_tool(const ToolDefinition& tool);

    // Start listening on port (launches background thread)
    Result<bool> start(uint16_t port = 3000);

    // Stop the server
    void stop();

    bool is_running() const { return running_.load(); }

private:
    void server_loop();
    void handle_client(intptr_t client_socket);
    JsonRpcResponse dispatch(const JsonRpcRequest& request);
    JsonRpcResponse handle_list_tools();

    // Parse a JSON-RPC request from raw string
    static Result<JsonRpcRequest> parse_request(const std::string& json);

    std::unordered_map<std::string, ToolDefinition> tools_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    intptr_t server_socket_ = -1;
    uint16_t port_ = 3000;
    std::mutex tools_mutex_;
};

} // namespace odyssey::mcp
