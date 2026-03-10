#pragma once

#include "core/result.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace odyssey::nadir {

struct ShaderBytecode {
    bool success = false;
    std::vector<uint32_t> spirv;
    std::string error_message;
    std::string shader_name;
};

// Pure: compile GLSL source to SPIR-V (uses shaderc, which is deterministic)
ShaderBytecode compile_behavior_shader(
    std::string_view glsl_source,
    std::string_view shader_name,
    const std::filesystem::path& include_dir
);

// Pure: generate the standard preamble that all .nadir files get prepended with.
// This includes the layout declarations for the standard SSBO bindings.
std::string generate_shader_preamble(uint32_t workgroup_size = 256);

// Pure: wrap raw nadir source with preamble to make complete GLSL
std::string prepare_shader_source(
    std::string_view nadir_source,
    uint32_t workgroup_size = 256
);

// Impure: read a .nadir file from disk and compile it
Result<ShaderBytecode> compile_nadir_file(
    const std::filesystem::path& nadir_path,
    const std::filesystem::path& lib_dir
);

} // namespace odyssey::nadir
