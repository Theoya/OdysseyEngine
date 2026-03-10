#include "assets/material_loader.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace odyssey::assets {

static float parse_float(const char* str, float default_val) {
    if (!str || str[0] == '\0') return default_val;
    try {
        return std::stof(str);
    } catch (...) {
        return default_val;
    }
}

static vec4 parse_vec4(const std::string& str, vec4 default_val) {
    if (str.empty()) return default_val;

    std::istringstream iss(str);
    vec4 result = default_val;
    iss >> result.x >> result.y >> result.z >> result.w;
    return result;
}

Result<MaterialData> parse_material_xml(const std::string& xml_content) {
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_string(xml_content.c_str());

    if (!parse_result) {
        return Result<MaterialData>::err(
            std::string("Failed to parse material XML: ") + parse_result.description());
    }

    auto mat_node = doc.child("material");
    if (!mat_node) {
        return Result<MaterialData>::err("Missing root <material> element");
    }

    MaterialData material;
    material.name = mat_node.attribute("name").as_string("unnamed");
    material.version = mat_node.attribute("version").as_int(1);

    // Shaders
    auto shaders_node = mat_node.child("shaders");
    if (shaders_node) {
        auto vert_node = shaders_node.child("vertex");
        if (vert_node) {
            material.vertex_shader = vert_node.attribute("src").as_string("");
        }
        auto frag_node = shaders_node.child("fragment");
        if (frag_node) {
            material.fragment_shader = frag_node.attribute("src").as_string("");
        }
    }

    // PBR properties
    auto pbr_node = mat_node.child("pbr");
    if (pbr_node) {
        auto albedo_node = pbr_node.child("albedo");
        if (albedo_node) {
            const char* color_str = albedo_node.attribute("color").as_string(nullptr);
            if (color_str) {
                material.albedo = parse_vec4(color_str, vec4{1.0f});
            }
        }
        auto metallic_node = pbr_node.child("metallic");
        if (metallic_node) {
            material.metallic = metallic_node.attribute("value").as_float(0.0f);
        }
        auto roughness_node = pbr_node.child("roughness");
        if (roughness_node) {
            material.roughness = roughness_node.attribute("value").as_float(1.0f);
        }
    }

    // Textures
    auto textures_node = mat_node.child("textures");
    if (textures_node) {
        auto albedo_tex = textures_node.child("albedo");
        if (albedo_tex) {
            material.albedo_map = albedo_tex.attribute("src").as_string("");
        }
        auto normal_tex = textures_node.child("normal");
        if (normal_tex) {
            material.normal_map = normal_tex.attribute("src").as_string("");
        }
        auto metallic_tex = textures_node.child("metallic");
        if (metallic_tex) {
            material.metallic_map = metallic_tex.attribute("src").as_string("");
        }
        auto roughness_tex = textures_node.child("roughness");
        if (roughness_tex) {
            material.roughness_map = roughness_tex.attribute("src").as_string("");
        }
    }

    spdlog::info("Parsed material '{}'", material.name);
    return Result<MaterialData>::ok(std::move(material));
}

Result<MaterialData> load_material_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return Result<MaterialData>::err("Material file not found: " + path.string());
    }

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        return Result<MaterialData>::err("Failed to open material file: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    spdlog::info("Loading material from '{}'", path.string());
    return parse_material_xml(content);
}

} // namespace odyssey::assets
