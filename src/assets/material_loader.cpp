#include "assets/material_loader.h"
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <functional>
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
        // NOTE: metallic/roughness fields are parsed for round-trip but not stored.
        // We do not forward these to MaterialGPU — adding PBR-coded material schema
        // fields re-triggers council (vibe-story-guardian condition).
        // auto metallic_node = pbr_node.child("metallic"); // intentionally unused
        // auto roughness_node = pbr_node.child("roughness"); // intentionally unused
        (void)pbr_node; // suppress unused warning after albedo extraction
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

// ---------------------------------------------------------------------------
// material_resolve_err_to_string
// ---------------------------------------------------------------------------

std::string material_resolve_err_to_string(MaterialResolveErr e) {
    switch (e) {
        case MaterialResolveErr::TextureLoadFailed: return "MaterialResolveErr::TextureLoadFailed";
        case MaterialResolveErr::RegistryFull:      return "MaterialResolveErr::RegistryFull";
        case MaterialResolveErr::IndexOutOfRange:   return "MaterialResolveErr::IndexOutOfRange";
    }
    return "MaterialResolveErr::Unknown";
}

// ---------------------------------------------------------------------------
// resolve_material_gpu
// ---------------------------------------------------------------------------

Result<MaterialGPU, MaterialResolveErr> resolve_material_gpu(
    const MaterialData& material,
    vulkan::BindlessTextureRegistry& registry,
    VkCommandPool command_pool,
    std::function<std::vector<uint8_t>(const std::filesystem::path&, uint32_t&, uint32_t&)> tex_load)
{
    MaterialGPU gpu;
    gpu.albedo_r = material.albedo.r;
    gpu.albedo_g = material.albedo.g;
    gpu.albedo_b = material.albedo.b;
    gpu.albedo_a = material.albedo.a;
    gpu.albedo_tex_index = 0u; // default: sentinel slot = no texture, use albedo color

    if (!material.albedo_map.empty()) {
        std::filesystem::path abs_path =
            std::filesystem::absolute(std::filesystem::path(material.albedo_map));

        // Check if already in registry (dedup).
        vulkan::TextureHandle existing = registry.find(abs_path);
        if (existing.is_valid()) {
            gpu.albedo_tex_index = existing.slot();
        } else {
            // Load pixels.
            uint32_t w = 0, h = 0;
            std::vector<uint8_t> pixels = tex_load(abs_path, w, h);
            if (pixels.empty()) {
                spdlog::warn("Material '{}': albedo texture '{}' failed to load — using sentinel",
                             material.name, abs_path.string());
                return Result<MaterialGPU, MaterialResolveErr>::err(
                    MaterialResolveErr::TextureLoadFailed);
            }

            auto reg_result = registry.load(abs_path, pixels.data(), w, h, command_pool);
            if (reg_result.is_err()) {
                return Result<MaterialGPU, MaterialResolveErr>::err(
                    MaterialResolveErr::RegistryFull);
            }

            gpu.albedo_tex_index = reg_result.value().slot();
        }

        // Sanity: slot must be within the bindless array bounds.
        if (gpu.albedo_tex_index >= vulkan::MAX_BINDLESS_TEXTURES) {
            return Result<MaterialGPU, MaterialResolveErr>::err(
                MaterialResolveErr::IndexOutOfRange);
        }
    }

    spdlog::debug("MaterialGPU resolved: '{}' albedo_tex_index={}", material.name, gpu.albedo_tex_index);
    return Result<MaterialGPU, MaterialResolveErr>::ok(gpu);
}

} // namespace odyssey::assets
