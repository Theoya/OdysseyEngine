#include "nadir/behavior_compiler.h"

#include <shaderc/shaderc.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>

namespace odyssey::nadir {

// ---------------------------------------------------------------------------
// Custom shaderc includer — resolves #include paths relative to a library
// directory so that .nadir files can #include "scoring.glsl" etc.
// ---------------------------------------------------------------------------
class NadirIncluder : public shaderc::CompileOptions::IncluderInterface {
public:
    explicit NadirIncluder(std::filesystem::path include_dir)
        : include_dir_(std::move(include_dir)) {}

    shaderc_include_result* GetInclude(
        const char* requested_source,
        shaderc_include_type /* type */,
        const char* /* requesting_source */,
        size_t /* include_depth */) override
    {
        auto* result = new shaderc_include_result{};

        std::filesystem::path full_path = include_dir_ / requested_source;

        std::ifstream file(full_path, std::ios::in);
        if (!file.is_open()) {
            auto* err = new std::string("Failed to open include file: " + full_path.string());
            result->source_name = "";
            result->source_name_length = 0;
            result->content = err->c_str();
            result->content_length = err->size();
            result->user_data = err;
            return result;
        }

        std::ostringstream ss;
        ss << file.rdbuf();

        auto* content = new std::string(ss.str());
        auto* name = new std::string(full_path.string());

        result->source_name = name->c_str();
        result->source_name_length = name->size();
        result->content = content->c_str();
        result->content_length = content->size();

        // Pack both allocations into user_data so we can free them later.
        // We store them as a pair allocated on the heap.
        auto* pair = new std::pair<std::string*, std::string*>(name, content);
        result->user_data = pair;

        return result;
    }

    void ReleaseInclude(shaderc_include_result* data) override {
        if (data->user_data) {
            // Determine which kind of user_data we stored.
            // If source_name_length == 0, we stored a single error string.
            if (data->source_name_length == 0) {
                delete static_cast<std::string*>(data->user_data);
            } else {
                auto* pair = static_cast<std::pair<std::string*, std::string*>*>(data->user_data);
                delete pair->first;
                delete pair->second;
                delete pair;
            }
        }
        delete data;
    }

private:
    std::filesystem::path include_dir_;
};

// ---------------------------------------------------------------------------
// Shader preamble — prepended to every .nadir file before compilation
// ---------------------------------------------------------------------------
std::string generate_shader_preamble(uint32_t workgroup_size) {
    std::ostringstream s;

    s << "#version 450\n";
    s << "layout(local_size_x = " << workgroup_size << ") in;\n";
    s << "\n";

    // ----- GPU struct definitions (must match C++ types exactly) -----

    s << "// GPU struct definitions matching C++ types with std430 alignment\n";
    s << "\n";

    s << "struct EntityStatsGPU {\n";
    s << "    float health;\n";
    s << "    float max_health;\n";
    s << "    float ammo;\n";
    s << "    float stamina;\n";
    s << "    float speed;\n";
    s << "    float _pad0;\n";
    s << "    float _pad1;\n";
    s << "    float _pad2;\n";
    s << "};\n";
    s << "\n";

    s << "struct AgentPersistGPU {\n";
    s << "    uint current_state;\n";
    s << "    float state_timer;\n";
    s << "    float cooldown_0;\n";
    s << "    float cooldown_1;\n";
    s << "    vec4 memory_0;\n";
    s << "    vec4 memory_1;\n";
    s << "    uint last_decision;\n";
    s << "    float _pad0;\n";
    s << "    float _pad1;\n";
    s << "    float _pad2;\n";
    s << "};\n";
    s << "\n";

    s << "struct BehaviorOutputGPU {\n";
    s << "    vec4 move_vector;\n";
    s << "    vec4 attack_target;\n";
    s << "    uint animation_id;\n";
    s << "    float animation_blend;\n";
    s << "    uint sound_event;\n";
    s << "    float sound_priority;\n";
    s << "    float comms_signal;\n";
    s << "    float comms_urgency;\n";
    s << "    uint action_request;   // sequence ID to start (0 = none)\n";
    s << "    float action_priority; // priority of the request\n";
    s << "    // total: 64 bytes\n";
    s << "};\n";
    s << "\n";

    // ----- SSBO / UBO bindings -----

    s << "// Buffer 0: Entity Transforms (read-only)\n";
    s << "layout(std430, set = 0, binding = 0) readonly buffer TransformBuffer {\n";
    s << "    vec4 positions[];  // xyz = position, w = unused\n";
    s << "};\n";
    s << "\n";

    s << "// Buffer 1: Entity Stats (read-only)\n";
    s << "layout(std430, set = 0, binding = 1) readonly buffer StatsBuffer {\n";
    s << "    EntityStatsGPU stats[];\n";
    s << "};\n";
    s << "\n";

    s << "// Buffer 2: Spatial Grid (read-only)\n";
    s << "layout(std430, set = 0, binding = 2) readonly buffer SpatialBuffer {\n";
    s << "    uvec4 spatial_cells[];\n";
    s << "};\n";
    s << "\n";

    s << "// Buffer 3: World State (uniform, read-only)\n";
    s << "layout(std140, set = 0, binding = 3) uniform WorldStateUBO {\n";
    s << "    float world_time;\n";
    s << "    float delta_time;\n";
    s << "    vec4 player_position;\n";
    s << "    uint frame_number;\n";
    s << "    uint total_entities;\n";
    s << "};\n";
    s << "\n";

    s << "// Buffer 4: Agent Persistent State (read/write)\n";
    s << "layout(std430, set = 0, binding = 4) buffer PersistBuffer {\n";
    s << "    AgentPersistGPU persist[];\n";
    s << "};\n";
    s << "\n";

    s << "// Buffer 5: Behavior Output (write-only)\n";
    s << "layout(std430, set = 0, binding = 5) writeonly buffer OutputBuffer {\n";
    s << "    BehaviorOutputGPU outputs[];\n";
    s << "};\n";
    s << "\n";

    s << "// Buffer 6: Debug Output (write-only)\n";
    s << "layout(std430, set = 0, binding = 6) writeonly buffer DebugBuffer {\n";
    s << "    vec4 debug_data[];\n";
    s << "};\n";
    s << "\n";

    return s.str();
}

// ---------------------------------------------------------------------------
std::string prepare_shader_source(std::string_view nadir_source,
                                  uint32_t workgroup_size) {
    std::string preamble = generate_shader_preamble(workgroup_size);
    preamble += "// ---- end preamble, begin .nadir source ----\n\n";
    preamble += nadir_source;
    return preamble;
}

// ---------------------------------------------------------------------------
ShaderBytecode compile_behavior_shader(std::string_view glsl_source,
                                       std::string_view shader_name,
                                       const std::filesystem::path& include_dir) {
    ShaderBytecode result;
    result.shader_name = std::string(shader_name);

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
    options.SetIncluder(std::make_unique<NadirIncluder>(include_dir));

    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
        glsl_source.data(),
        glsl_source.size(),
        shaderc_compute_shader,
        std::string(shader_name).c_str(),
        options
    );

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        result.success = false;
        result.error_message = module.GetErrorMessage();
        spdlog::error("Shader compilation failed for '{}': {}", shader_name,
                      result.error_message);
        return result;
    }

    result.spirv = std::vector<uint32_t>(module.cbegin(), module.cend());
    result.success = true;

    spdlog::info("Compiled behavior shader '{}' — {} SPIR-V words",
                 shader_name, result.spirv.size());
    return result;
}

// ---------------------------------------------------------------------------
Result<ShaderBytecode> compile_nadir_file(const std::filesystem::path& nadir_path,
                                          const std::filesystem::path& lib_dir) {
    // Read the .nadir source file
    std::ifstream file(nadir_path, std::ios::in);
    if (!file.is_open()) {
        return Result<ShaderBytecode>::err(
            "Failed to open .nadir file: " + nadir_path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string raw_source = ss.str();

    // Prepend the standard preamble
    std::string full_source = prepare_shader_source(raw_source);

    // Compile
    std::string name = nadir_path.filename().string();
    ShaderBytecode bytecode = compile_behavior_shader(full_source, name, lib_dir);

    if (!bytecode.success) {
        return Result<ShaderBytecode>::err(
            "Compilation failed for " + name + ": " + bytecode.error_message);
    }

    return Result<ShaderBytecode>::ok(std::move(bytecode));
}

} // namespace odyssey::nadir
