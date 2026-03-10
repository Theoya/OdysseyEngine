#pragma once
#include "core/types.h"
#include "core/result.h"
#include <string>
#include <filesystem>

namespace odyssey::assets {

struct MaterialData {
    std::string name;
    int version = 1;

    // Shader paths
    std::string vertex_shader;
    std::string fragment_shader;

    // PBR properties
    vec4 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;

    // Texture paths
    std::string albedo_map;
    std::string normal_map;
    std::string metallic_map;
    std::string roughness_map;
};

// Pure: parse material XML
Result<MaterialData> parse_material_xml(const std::string& xml_content);

// Impure: load from file
Result<MaterialData> load_material_file(const std::filesystem::path& path);

} // namespace odyssey::assets
