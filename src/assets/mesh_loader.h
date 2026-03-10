#pragma once
#include "core/types.h"
#include "core/result.h"
#include <string>
#include <filesystem>
#include <vector>

namespace odyssey::assets {

struct LODLevel {
    float distance = 0.0f;
    uint32_t triangles = 0;
};

struct MeshData {
    std::string name;
    int version = 1;
    std::string source_format;  // "glTF", "OBJ"
    std::filesystem::path source_path;
    std::vector<LODLevel> lod_levels;

    // Collider
    std::string collider_type;
    bool auto_fit = false;

    // Loaded geometry (populated after loading source)
    std::vector<float> vertices;     // interleaved pos+normal+uv
    std::vector<uint32_t> indices;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
};

// Pure: parse mesh XML descriptor
Result<MeshData> parse_mesh_xml(const std::string& xml_content);

// Impure: load mesh descriptor from file
Result<MeshData> load_mesh_descriptor(const std::filesystem::path& path);

// Impure: load actual geometry from source file (OBJ via tinyobjloader)
Result<MeshData> load_mesh_geometry(MeshData& mesh, const std::filesystem::path& base_dir);

} // namespace odyssey::assets
