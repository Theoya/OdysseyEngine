#pragma once

#include "mcp/mcp_server.h"

#include <vector>

namespace odyssey::mcp {

// Forward declarations for engine subsystems.
// The tools take raw pointers to engine subsystems set at init time.

struct ToolContext {
    // Pointers set by engine during init (nullable if subsystem not ready)
    void* entity_manager = nullptr;  // odyssey::scene::EntityManager*
    void* nadir_system = nullptr;    // odyssey::nadir::NadirSystem*
    void* engine = nullptr;          // odyssey::Engine*
};

// Register all OdysseyEngine MCP tools with the server.
void register_engine_tools(MCPServer& server, ToolContext& ctx);

// Individual tool implementations
namespace tools {

// Scene tools
JsonRpcResponse scene_get_entities(const JsonRpcRequest& req, ToolContext& ctx);
JsonRpcResponse entity_inspect(const JsonRpcRequest& req, ToolContext& ctx);
JsonRpcResponse entity_modify(const JsonRpcRequest& req, ToolContext& ctx);
JsonRpcResponse entity_spawn(const JsonRpcRequest& req, ToolContext& ctx);
JsonRpcResponse entity_destroy(const JsonRpcRequest& req, ToolContext& ctx);

// Nadir tools
JsonRpcResponse nadir_compile(const JsonRpcRequest& req, ToolContext& ctx);
JsonRpcResponse nadir_hot_reload(const JsonRpcRequest& req, ToolContext& ctx);
JsonRpcResponse nadir_list(const JsonRpcRequest& req, ToolContext& ctx);

// Engine tools
JsonRpcResponse engine_status(const JsonRpcRequest& req, ToolContext& ctx);
JsonRpcResponse frame_profile(const JsonRpcRequest& req, ToolContext& ctx);

} // namespace tools
} // namespace odyssey::mcp
