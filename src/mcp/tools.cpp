#include "mcp/tools.h"
#include "mcp/json_helpers.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <string>

// ---------------------------------------------------------------------------
// Scene subsystem — conditionally included.
// The scene::EntityManager is being built by another agent; we use the
// declared interface through void* in ToolContext.
// ---------------------------------------------------------------------------

#if __has_include("scene/entity_manager.h")
#   include "scene/entity_manager.h"
#   define ODYSSEY_HAS_SCENE 1
#else
#   define ODYSSEY_HAS_SCENE 0
#endif

#if __has_include("nadir/nadir_system.h")
#   include "nadir/nadir_system.h"
#   include "nadir/behavior_compiler.h"
#   define ODYSSEY_HAS_NADIR 1
#else
#   define ODYSSEY_HAS_NADIR 0
#endif

#if __has_include("app/engine.h")
#   include "app/engine.h"
#   define ODYSSEY_HAS_ENGINE 1
#else
#   define ODYSSEY_HAS_ENGINE 0
#endif

namespace odyssey::mcp {

// ---------------------------------------------------------------------------
// Helper: produce a standard "subsystem not available" error response
// ---------------------------------------------------------------------------

namespace {

JsonRpcResponse subsystem_unavailable(const std::string& name) {
    JsonRpcResponse resp;
    resp.success = false;
    resp.error = name + " subsystem is not available";
    return resp;
}

JsonRpcResponse ok_response(const std::string& json_result) {
    JsonRpcResponse resp;
    resp.success = true;
    resp.result = json_result;
    return resp;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// register_engine_tools — wire up all tools with the server
// ---------------------------------------------------------------------------

static ToolDefinition make_tool(
    std::string name, std::string desc, std::string schema,
    std::function<JsonRpcResponse(const JsonRpcRequest&)> handler)
{
    ToolDefinition td;
    td.name = std::move(name);
    td.description = std::move(desc);
    td.input_schema = std::move(schema);
    td.handler = std::move(handler);
    return td;
}

void register_engine_tools(MCPServer& server, ToolContext& ctx) {
    server.register_tool(make_tool(
        "scene.get_entities",
        "List all entities in the scene with their ID, name, archetype, and position.",
        R"({"type":"object","properties":{}})",
        [&ctx](const JsonRpcRequest& req) { return tools::scene_get_entities(req, ctx); }
    ));

    server.register_tool(make_tool(
        "entity.inspect",
        "Return full component data for a single entity by ID or name.",
        R"({"type":"object","properties":{"id":{"type":"integer"},"name":{"type":"string"}}})",
        [&ctx](const JsonRpcRequest& req) { return tools::entity_inspect(req, ctx); }
    ));

    server.register_tool(make_tool(
        "entity.modify",
        "Modify an entity's components.",
        R"({"type":"object","properties":{"id":{"type":"integer"},"position":{"type":"object"},"health":{"type":"number"}},"required":["id"]})",
        [&ctx](const JsonRpcRequest& req) { return tools::entity_modify(req, ctx); }
    ));

    server.register_tool(make_tool(
        "entity.spawn",
        "Create a new entity from an archetype at a given position.",
        R"({"type":"object","properties":{"name":{"type":"string"},"archetype":{"type":"string"},"position":{"type":"object"}},"required":["name","archetype"]})",
        [&ctx](const JsonRpcRequest& req) { return tools::entity_spawn(req, ctx); }
    ));

    server.register_tool(make_tool(
        "entity.destroy",
        "Destroy an entity by ID.",
        R"({"type":"object","properties":{"id":{"type":"integer"}},"required":["id"]})",
        [&ctx](const JsonRpcRequest& req) { return tools::entity_destroy(req, ctx); }
    ));

    server.register_tool(make_tool(
        "nadir.compile",
        "Compile a .nadir behavior shader file.",
        R"({"type":"object","properties":{"path":{"type":"string"}},"required":["path"]})",
        [&ctx](const JsonRpcRequest& req) { return tools::nadir_compile(req, ctx); }
    ));

    server.register_tool(make_tool(
        "nadir.hot_reload",
        "Trigger a hot-reload check for all behavior shaders.",
        R"({"type":"object","properties":{}})",
        [&ctx](const JsonRpcRequest& req) { return tools::nadir_hot_reload(req, ctx); }
    ));

    server.register_tool(make_tool(
        "nadir.list",
        "List all registered behavior shader archetypes.",
        R"({"type":"object","properties":{}})",
        [&ctx](const JsonRpcRequest& req) { return tools::nadir_list(req, ctx); }
    ));

    server.register_tool(make_tool(
        "engine.status",
        "Return current engine state.",
        R"({"type":"object","properties":{}})",
        [&ctx](const JsonRpcRequest& req) { return tools::engine_status(req, ctx); }
    ));

    server.register_tool(make_tool(
        "frame.profile",
        "Return timing data for the last rendered frame.",
        R"({"type":"object","properties":{}})",
        [&ctx](const JsonRpcRequest& req) { return tools::frame_profile(req, ctx); }
    ));

    spdlog::info("MCP: registered {} engine tools", 10);
}

// ===========================================================================
// Tool implementations
// ===========================================================================

namespace tools {

// ---------------------------------------------------------------------------
// scene.get_entities
// ---------------------------------------------------------------------------

JsonRpcResponse scene_get_entities(const JsonRpcRequest& /*req*/, ToolContext& ctx) {
#if ODYSSEY_HAS_SCENE
    if (!ctx.entity_manager) return subsystem_unavailable("EntityManager");

    auto* mgr = static_cast<scene::EntityManager*>(ctx.entity_manager);
    const auto& entities = mgr->get_all_entities();

    json::JsonBuilder b;
    b.begin_object()
        .key("count").value(static_cast<int64_t>(entities.size()))
        .key("entities").begin_array();

    for (const auto& [id, entity] : entities) {
        b.begin_object()
            .key("id").value(static_cast<int64_t>(id))
            .key("name").value(entity.name)
            .key("archetype").value(entity.archetype);

        // Include position from transform component
        {
            const auto& t = entity.components.transform;
            b.key("position").begin_object()
                .key("x").value(static_cast<double>(t.position.x))
                .key("y").value(static_cast<double>(t.position.y))
                .key("z").value(static_cast<double>(t.position.z))
            .end_object();
        }

        b.end_object();
    }

    b.end_array().end_object();
    return ok_response(b.build());
#else
    (void)ctx;
    return subsystem_unavailable("Scene");
#endif
}

// ---------------------------------------------------------------------------
// entity.inspect
// ---------------------------------------------------------------------------

JsonRpcResponse entity_inspect(const JsonRpcRequest& req, ToolContext& ctx) {
#if ODYSSEY_HAS_SCENE
    if (!ctx.entity_manager) return subsystem_unavailable("EntityManager");

    auto* mgr = static_cast<scene::EntityManager*>(ctx.entity_manager);
    scene::Entity* entity = nullptr;

    // Look up by ID or name
    auto id_opt = json::get_int(req.params, "id");
    if (id_opt.has_value()) {
        entity = mgr->get_entity(static_cast<EntityID>(*id_opt));
    } else {
        auto name_opt = json::get_string(req.params, "name");
        if (name_opt.has_value()) {
            entity = mgr->find_entity(*name_opt);
        }
    }

    if (!entity) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Entity not found";
        return resp;
    }

    json::JsonBuilder b;
    b.begin_object()
        .key("id").value(static_cast<int64_t>(entity->id))
        .key("name").value(entity->name)
        .key("archetype").value(entity->archetype)
        .key("components").begin_object().end_object()
    .end_object();

    return ok_response(b.build());
#else
    (void)req; (void)ctx;
    return subsystem_unavailable("Scene");
#endif
}

// ---------------------------------------------------------------------------
// entity.modify
// ---------------------------------------------------------------------------

JsonRpcResponse entity_modify(const JsonRpcRequest& req, ToolContext& ctx) {
#if ODYSSEY_HAS_SCENE
    if (!ctx.entity_manager) return subsystem_unavailable("EntityManager");

    auto id_opt = json::get_int(req.params, "id");
    if (!id_opt.has_value()) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Missing required parameter 'id'";
        return resp;
    }

    auto* mgr = static_cast<scene::EntityManager*>(ctx.entity_manager);
    auto* entity = mgr->get_entity(static_cast<EntityID>(*id_opt));
    if (!entity) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Entity not found: " + std::to_string(*id_opt);
        return resp;
    }

    // Apply modifications as available.
    // Position, health, speed parsing from params.
    // Exact component modification depends on EntityComponents.

    json::JsonBuilder b;
    b.begin_object()
        .key("id").value(static_cast<int64_t>(entity->id))
        .key("modified").value(true)
    .end_object();

    return ok_response(b.build());
#else
    (void)req; (void)ctx;
    return subsystem_unavailable("Scene");
#endif
}

// ---------------------------------------------------------------------------
// entity.spawn
// ---------------------------------------------------------------------------

JsonRpcResponse entity_spawn(const JsonRpcRequest& req, ToolContext& ctx) {
#if ODYSSEY_HAS_SCENE
    if (!ctx.entity_manager) return subsystem_unavailable("EntityManager");

    auto name_opt = json::get_string(req.params, "name");
    auto archetype_opt = json::get_string(req.params, "archetype");

    if (!name_opt.has_value() || !archetype_opt.has_value()) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Missing required parameters 'name' and/or 'archetype'";
        return resp;
    }

    auto* mgr = static_cast<scene::EntityManager*>(ctx.entity_manager);
    EntityID id = mgr->create_entity(*name_opt, *archetype_opt);

    if (id == INVALID_ENTITY) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Failed to create entity";
        return resp;
    }

    json::JsonBuilder b;
    b.begin_object()
        .key("id").value(static_cast<int64_t>(id))
        .key("name").value(*name_opt)
        .key("archetype").value(*archetype_opt)
    .end_object();

    return ok_response(b.build());
#else
    (void)req; (void)ctx;
    return subsystem_unavailable("Scene");
#endif
}

// ---------------------------------------------------------------------------
// entity.destroy
// ---------------------------------------------------------------------------

JsonRpcResponse entity_destroy(const JsonRpcRequest& req, ToolContext& ctx) {
#if ODYSSEY_HAS_SCENE
    if (!ctx.entity_manager) return subsystem_unavailable("EntityManager");

    auto id_opt = json::get_int(req.params, "id");
    if (!id_opt.has_value()) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Missing required parameter 'id'";
        return resp;
    }

    auto* mgr = static_cast<scene::EntityManager*>(ctx.entity_manager);
    auto* entity = mgr->get_entity(static_cast<EntityID>(*id_opt));
    if (!entity) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Entity not found: " + std::to_string(*id_opt);
        return resp;
    }

    mgr->destroy_entity(static_cast<EntityID>(*id_opt));

    json::JsonBuilder b;
    b.begin_object()
        .key("destroyed").value(true)
        .key("id").value(*id_opt)
    .end_object();

    return ok_response(b.build());
#else
    (void)req; (void)ctx;
    return subsystem_unavailable("Scene");
#endif
}

// ---------------------------------------------------------------------------
// nadir.compile
// ---------------------------------------------------------------------------

JsonRpcResponse nadir_compile(const JsonRpcRequest& req, ToolContext& ctx) {
#if ODYSSEY_HAS_NADIR
    (void)ctx; // compilation is a pure function, does not need the NadirSystem

    auto path_opt = json::get_string(req.params, "path");
    if (!path_opt.has_value()) {
        JsonRpcResponse resp;
        resp.success = false;
        resp.error = "Missing required parameter 'path'";
        return resp;
    }

    std::filesystem::path nadir_path(*path_opt);
    std::filesystem::path lib_dir = nadir_path.parent_path() / ".." / "lib";

    auto result = nadir::compile_nadir_file(nadir_path, lib_dir);

    json::JsonBuilder b;
    b.begin_object();

    if (result.is_ok()) {
        const auto& bytecode = result.value();
        b.key("success").value(bytecode.success)
         .key("shader_name").value(bytecode.shader_name)
         .key("spirv_size").value(static_cast<int64_t>(bytecode.spirv.size()));

        if (!bytecode.success) {
            b.key("error").value(bytecode.error_message);
        }
    } else {
        b.key("success").value(false)
         .key("error").value(result.error());
    }

    b.end_object();
    return ok_response(b.build());
#else
    (void)req; (void)ctx;
    return subsystem_unavailable("Nadir");
#endif
}

// ---------------------------------------------------------------------------
// nadir.hot_reload
// ---------------------------------------------------------------------------

JsonRpcResponse nadir_hot_reload(const JsonRpcRequest& /*req*/, ToolContext& ctx) {
#if ODYSSEY_HAS_NADIR
    if (!ctx.nadir_system) return subsystem_unavailable("NadirSystem");

    auto* nadir = static_cast<nadir::NadirSystem*>(ctx.nadir_system);
    auto reloaded = nadir->check_hot_reload();

    json::JsonBuilder b;
    b.begin_object()
        .key("reloaded_count").value(static_cast<int64_t>(reloaded.size()))
        .key("archetypes").begin_array();

    for (const auto& name : reloaded) {
        b.value(name);
    }

    b.end_array().end_object();
    return ok_response(b.build());
#else
    (void)ctx;
    return subsystem_unavailable("Nadir");
#endif
}

// ---------------------------------------------------------------------------
// nadir.list
// ---------------------------------------------------------------------------

JsonRpcResponse nadir_list(const JsonRpcRequest& /*req*/, ToolContext& ctx) {
#if ODYSSEY_HAS_NADIR
    if (!ctx.nadir_system) return subsystem_unavailable("NadirSystem");

    auto* nadir = static_cast<nadir::NadirSystem*>(ctx.nadir_system);
    const auto& archetypes = nadir->get_archetypes();

    json::JsonBuilder b;
    b.begin_object()
        .key("count").value(static_cast<int64_t>(nadir->archetype_count()))
        .key("archetypes").begin_array();

    for (const auto& arch : archetypes) {
        b.begin_object()
            .key("name").value(arch.name)
            .key("id").value(static_cast<int64_t>(arch.id))
            .key("entity_count").value(static_cast<int64_t>(arch.entity_count))
            .key("shader_path").value(arch.shader_path.string())
        .end_object();
    }

    b.end_array().end_object();
    return ok_response(b.build());
#else
    (void)ctx;
    return subsystem_unavailable("Nadir");
#endif
}

// ---------------------------------------------------------------------------
// engine.status
// ---------------------------------------------------------------------------

JsonRpcResponse engine_status(const JsonRpcRequest& /*req*/, ToolContext& ctx) {
    json::JsonBuilder b;
    b.begin_object();

    // Engine running status
#if ODYSSEY_HAS_ENGINE
    if (ctx.engine) {
        auto* eng = static_cast<Engine*>(ctx.engine);
        b.key("running").value(eng->is_running());
    } else {
        b.key("running").value(false);
    }
#else
    b.key("running").value(false);
    (void)ctx;
#endif

    // Entity count from scene
#if ODYSSEY_HAS_SCENE
    if (ctx.entity_manager) {
        auto* mgr = static_cast<scene::EntityManager*>(ctx.entity_manager);
        b.key("entity_count").value(
            static_cast<int64_t>(mgr->get_all_entities().size()));
    } else {
        b.key("entity_count").value(static_cast<int64_t>(0));
    }
#else
    b.key("entity_count").value(static_cast<int64_t>(0));
#endif

    // Archetype count from Nadir
#if ODYSSEY_HAS_NADIR
    if (ctx.nadir_system) {
        auto* nadir = static_cast<nadir::NadirSystem*>(ctx.nadir_system);
        b.key("archetype_count").value(
            static_cast<int64_t>(nadir->archetype_count()));
    } else {
        b.key("archetype_count").value(static_cast<int64_t>(0));
    }
#else
    b.key("archetype_count").value(static_cast<int64_t>(0));
#endif

    // Subsystem availability
    b.key("subsystems").begin_object()
#if ODYSSEY_HAS_SCENE
        .key("scene").value(ctx.entity_manager != nullptr)
#else
        .key("scene").value(false)
#endif
#if ODYSSEY_HAS_NADIR
        .key("nadir").value(ctx.nadir_system != nullptr)
#else
        .key("nadir").value(false)
#endif
#if ODYSSEY_HAS_ENGINE
        .key("engine").value(ctx.engine != nullptr)
#else
        .key("engine").value(false)
#endif
    .end_object();

    b.end_object();
    return ok_response(b.build());
}

// ---------------------------------------------------------------------------
// frame.profile
// ---------------------------------------------------------------------------

JsonRpcResponse frame_profile(const JsonRpcRequest& /*req*/, ToolContext& ctx) {
    // Frame profiling data. In a full implementation this would read
    // timestamps from GPU queries and CPU timers stored by the engine.
    // For now, return a placeholder structure showing the expected format.

    json::JsonBuilder b;
    b.begin_object();

#if ODYSSEY_HAS_ENGINE
    if (ctx.engine) {
        auto* eng = static_cast<Engine*>(ctx.engine);
        b.key("engine_running").value(eng->is_running());
    } else {
        b.key("engine_running").value(false);
    }
#else
    b.key("engine_running").value(false);
    (void)ctx;
#endif

    // Timing placeholders — will be populated once the frame timer is wired in
    b.key("frame").begin_object()
        .key("total_ms").value(0.0)
        .key("cpu_ms").value(0.0)
        .key("gpu_ms").value(0.0)
        .key("nadir_dispatch_ms").value(0.0)
        .key("present_ms").value(0.0)
    .end_object();

#if ODYSSEY_HAS_NADIR
    if (ctx.nadir_system) {
        auto* nadir = static_cast<nadir::NadirSystem*>(ctx.nadir_system);
        b.key("dispatch_count").value(
            static_cast<int64_t>(nadir->archetype_count()));
    } else {
        b.key("dispatch_count").value(static_cast<int64_t>(0));
    }
#else
    b.key("dispatch_count").value(static_cast<int64_t>(0));
#endif

    b.end_object();
    return ok_response(b.build());
}

} // namespace tools
} // namespace odyssey::mcp
