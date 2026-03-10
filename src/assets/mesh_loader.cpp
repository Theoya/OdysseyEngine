#include "assets/mesh_loader.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <tiny_obj_loader.h>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace odyssey::assets {

static float parse_float(const char* str, float default_val) {
    if (!str || str[0] == '\0') return default_val;
    try {
        return std::stof(str);
    } catch (...) {
        return default_val;
    }
}

static uint32_t parse_uint(const char* str, uint32_t default_val) {
    if (!str || str[0] == '\0') return default_val;
    try {
        return static_cast<uint32_t>(std::stoul(str));
    } catch (...) {
        return default_val;
    }
}

Result<MeshData> parse_mesh_xml(const std::string& xml_content) {
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_string(xml_content.c_str());

    if (!parse_result) {
        return Result<MeshData>::err(
            std::string("Failed to parse mesh XML: ") + parse_result.description());
    }

    auto mesh_node = doc.child("mesh");
    if (!mesh_node) {
        return Result<MeshData>::err("Missing root <mesh> element");
    }

    MeshData mesh;
    mesh.name = mesh_node.attribute("name").as_string("unnamed");
    mesh.version = mesh_node.attribute("version").as_int(1);

    // Source
    auto source_node = mesh_node.child("source");
    if (source_node) {
        mesh.source_format = source_node.attribute("format").as_string("");
        mesh.source_path = source_node.attribute("path").as_string("");
    }

    // LOD levels
    auto lod_node = mesh_node.child("lod");
    if (lod_node) {
        for (auto level_node : lod_node.children("level")) {
            LODLevel level;
            level.distance = level_node.attribute("distance").as_float(0.0f);
            level.triangles = level_node.attribute("triangles").as_uint(0);
            mesh.lod_levels.push_back(level);
        }
    }

    // Collider
    auto collider_node = mesh_node.child("collider");
    if (collider_node) {
        mesh.collider_type = collider_node.attribute("type").as_string("");
        mesh.auto_fit = collider_node.attribute("auto_fit").as_bool(false);
    }

    spdlog::info("Parsed mesh descriptor '{}' (format: {}, {} LOD levels)",
                 mesh.name, mesh.source_format, mesh.lod_levels.size());
    return Result<MeshData>::ok(std::move(mesh));
}

Result<MeshData> load_mesh_descriptor(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<MeshData>::err("Mesh descriptor not found: " + path.string());
    }

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        return Result<MeshData>::err("Failed to open mesh descriptor: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    spdlog::info("Loading mesh descriptor from '{}'", path.string());
    return parse_mesh_xml(content);
}

// Hash for vertex deduplication
struct VertexHash {
    size_t operator()(const std::tuple<int, int, int>& t) const {
        auto h1 = std::hash<int>{}(std::get<0>(t));
        auto h2 = std::hash<int>{}(std::get<1>(t));
        auto h3 = std::hash<int>{}(std::get<2>(t));
        return h1 ^ (h2 << 16) ^ (h3 << 8);
    }
};

Result<MeshData> load_mesh_geometry(MeshData& mesh, const std::filesystem::path& base_dir) {
    if (mesh.source_format != "OBJ" && mesh.source_format != "obj") {
        return Result<MeshData>::err("Unsupported mesh format: " + mesh.source_format
                                     + " (only OBJ is supported)");
    }

    std::filesystem::path obj_path = base_dir / mesh.source_path;
    if (!std::filesystem::exists(obj_path)) {
        return Result<MeshData>::err("OBJ file not found: " + obj_path.string());
    }

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string obj_path_str = obj_path.string();
    std::string mtl_base_dir = base_dir.string() + "/";

    bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                                    obj_path_str.c_str(), mtl_base_dir.c_str());

    if (!warn.empty()) {
        spdlog::warn("OBJ loader warning: {}", warn);
    }

    if (!err.empty() || !success) {
        return Result<MeshData>::err("Failed to load OBJ file: " + err);
    }

    // Interleave vertices: position (3) + normal (3) + texcoord (2) = 8 floats per vertex
    constexpr uint32_t floats_per_vertex = 8;

    std::unordered_map<std::tuple<int, int, int>, uint32_t, VertexHash> unique_vertices;
    mesh.vertices.clear();
    mesh.indices.clear();

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            auto key = std::make_tuple(index.vertex_index, index.normal_index,
                                       index.texcoord_index);

            auto it = unique_vertices.find(key);
            if (it != unique_vertices.end()) {
                mesh.indices.push_back(it->second);
            } else {
                uint32_t new_index = static_cast<uint32_t>(
                    mesh.vertices.size() / floats_per_vertex);
                unique_vertices[key] = new_index;
                mesh.indices.push_back(new_index);

                // Position
                if (index.vertex_index >= 0) {
                    mesh.vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
                    mesh.vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
                    mesh.vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);
                } else {
                    mesh.vertices.push_back(0.0f);
                    mesh.vertices.push_back(0.0f);
                    mesh.vertices.push_back(0.0f);
                }

                // Normal
                if (index.normal_index >= 0
                    && static_cast<size_t>(3 * index.normal_index + 2) < attrib.normals.size()) {
                    mesh.vertices.push_back(attrib.normals[3 * index.normal_index + 0]);
                    mesh.vertices.push_back(attrib.normals[3 * index.normal_index + 1]);
                    mesh.vertices.push_back(attrib.normals[3 * index.normal_index + 2]);
                } else {
                    mesh.vertices.push_back(0.0f);
                    mesh.vertices.push_back(1.0f);
                    mesh.vertices.push_back(0.0f);
                }

                // Texcoord
                if (index.texcoord_index >= 0
                    && static_cast<size_t>(2 * index.texcoord_index + 1) < attrib.texcoords.size()) {
                    mesh.vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
                    mesh.vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 1]);
                } else {
                    mesh.vertices.push_back(0.0f);
                    mesh.vertices.push_back(0.0f);
                }
            }
        }
    }

    mesh.vertex_count = static_cast<uint32_t>(mesh.vertices.size() / floats_per_vertex);
    mesh.index_count = static_cast<uint32_t>(mesh.indices.size());

    spdlog::info("Loaded OBJ geometry '{}': {} vertices, {} indices",
                 mesh.name, mesh.vertex_count, mesh.index_count);
    return Result<MeshData>::ok(std::move(mesh));
}

} // namespace odyssey::assets
