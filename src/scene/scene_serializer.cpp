#include "scene/scene_serializer.h"

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

namespace odyssey::scene {

// ---------------------------------------------------------------------------
// Local formatting helpers (pure).
// ---------------------------------------------------------------------------

// Format a float the same way pugixml/hand-authored XML expresses it.
// We prefer the minimal representation: "1", "1.5", "-9.81", "0.75".
// Uses fixed notation with up to 6 decimals, trailing zeros trimmed.
static std::string fmt_float(float v) {
    if (v == 0.0f) return "0";
    std::ostringstream oss;
    oss.precision(6);
    oss << std::fixed << v;
    std::string s = oss.str();
    // Trim trailing zeros but keep at least one digit after the point.
    auto dot = s.find('.');
    if (dot != std::string::npos) {
        size_t last = s.find_last_not_of('0');
        if (last != std::string::npos && last > dot) {
            s.erase(last + 1);
        } else if (last == dot) {
            s.erase(dot);
        }
    }
    return s;
}

static std::string fmt_vec3(const vec3& v) {
    return fmt_float(v.x) + " " + fmt_float(v.y) + " " + fmt_float(v.z);
}

static std::string fmt_quat(const quat& q) {
    // XML convention in this codebase: "x y z w" (matches scene_loader parse_quat).
    return fmt_float(q.x) + " " + fmt_float(q.y) + " " +
           fmt_float(q.z) + " " + fmt_float(q.w);
}

// ---------------------------------------------------------------------------
// Reconstruction path (used when force_reconstruct=true or preserved_source
// is empty). Emits a deterministic XML layout with the stable field order.
//
// Ordering contract (documented in docs/architecture.md):
//   1. Known attributes first (in declared order: name, version, ...)
//   2. Unknown attributes next, in insertion order.
//   3. Known child elements next (world, entities, ...), in declared order.
//   4. Unknown children last, in insertion order.
// ---------------------------------------------------------------------------

static void append_entity(std::ostringstream& out,
                          const SceneData::EntityDesc& desc,
                          const std::string& indent) {
    out << indent << "<entity";
    if (!desc.id.empty())        out << " id=\"" << desc.id << "\"";
    if (!desc.archetype.empty()) out << " archetype=\"" << desc.archetype << "\"";
    if (desc.count != 1)         out << " count=\"" << desc.count << "\"";
    for (const auto& [k, v] : desc.unknown_attributes) {
        out << " " << k << "=\"" << v << "\"";
    }

    // Decide whether entity has any children.
    bool has_transform = (desc.transform.position != vec3{0.f} ||
                          desc.transform.rotation != quat{1.f, 0.f, 0.f, 0.f} ||
                          desc.transform.scale != vec3{1.f});
    bool has_stats = (desc.stats.health != 100.0f ||
                      desc.stats.max_health != 100.0f ||
                      desc.stats.ammo != 0.0f ||
                      desc.stats.stamina != 100.0f ||
                      desc.stats.speed != 5.0f);
    bool has_children = has_transform || has_stats ||
                        !desc.behavior_shader.empty() ||
                        !desc.mesh_src.empty() ||
                        !desc.material_src.empty() ||
                        !desc.script_class.empty() ||
                        !desc.spawn_type.empty() ||
                        !desc.pack_leader.empty() ||
                        !desc.unknown_children_xml.empty();
    if (!has_children) {
        out << "/>\n";
        return;
    }
    out << ">\n";

    const std::string inner = indent + "  ";
    if (has_transform) {
        out << inner << "<transform";
        if (desc.transform.position != vec3{0.f})
            out << " position=\"" << fmt_vec3(desc.transform.position) << "\"";
        if (desc.transform.rotation != quat{1.f, 0.f, 0.f, 0.f})
            out << " rotation=\"" << fmt_quat(desc.transform.rotation) << "\"";
        if (desc.transform.scale != vec3{1.f})
            out << " scale=\"" << fmt_vec3(desc.transform.scale) << "\"";
        out << "/>\n";
    }
    if (has_stats) {
        out << inner << "<stats";
        out << " health=\"" << fmt_float(desc.stats.health) << "\"";
        out << " max_health=\"" << fmt_float(desc.stats.max_health) << "\"";
        if (desc.stats.ammo != 0.0f)
            out << " ammo=\"" << fmt_float(desc.stats.ammo) << "\"";
        if (desc.stats.stamina != 100.0f)
            out << " stamina=\"" << fmt_float(desc.stats.stamina) << "\"";
        if (desc.stats.speed != 5.0f)
            out << " speed=\"" << fmt_float(desc.stats.speed) << "\"";
        out << "/>\n";
    }
    if (!desc.behavior_shader.empty()) {
        out << inner << "<behavior shader=\"" << desc.behavior_shader << "\"/>\n";
    }
    if (!desc.mesh_src.empty()) {
        out << inner << "<mesh src=\"" << desc.mesh_src << "\"/>\n";
    }
    if (!desc.material_src.empty()) {
        out << inner << "<material src=\"" << desc.material_src << "\"/>\n";
    }
    if (!desc.script_class.empty()) {
        out << inner << "<script class=\"" << desc.script_class << "\"";
        if (!desc.script_config.empty())
            out << " config=\"" << desc.script_config << "\"";
        out << "/>\n";
    }
    if (!desc.spawn_type.empty()) {
        out << inner << "<spawn_region type=\"" << desc.spawn_type << "\"";
        out << " center=\"" << fmt_vec3(desc.spawn_center) << "\"";
        if (desc.spawn_radius != 0.f)
            out << " radius=\"" << fmt_float(desc.spawn_radius) << "\"";
        out << "/>\n";
    }
    if (!desc.pack_leader.empty()) {
        out << inner << "<pack leader=\"" << desc.pack_leader << "\"/>\n";
    }
    for (const auto& raw : desc.unknown_children_xml) {
        out << inner << raw;
        if (!raw.empty() && raw.back() != '\n') out << '\n';
    }
    out << indent << "</entity>\n";
}

static std::string reconstruct_scene_xml(const SceneData& scene,
                                         const SerializeOptions& options) {
    std::ostringstream out;
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<scene";
    if (!scene.name.empty()) out << " name=\"" << scene.name << "\"";
    out << " version=\"" << scene.version << "\"";
    for (const auto& [k, v] : scene.unknown_scene_attributes) {
        out << " " << k << "=\"" << v << "\"";
    }
    out << ">\n";

    // World
    out << options.indent << "<world>\n";
    out << options.indent << options.indent
        << "<time_scale>" << fmt_float(scene.time_scale) << "</time_scale>\n";
    out << options.indent << options.indent
        << "<gravity>" << fmt_vec3(scene.gravity) << "</gravity>\n";
    out << options.indent << "</world>\n";

    for (const auto& e : scene.entities) {
        append_entity(out, e, options.indent);
    }
    out << "</scene>\n";
    return out.str();
}

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------

Result<std::string> serialize_scene_to_string(
    const SceneData& scene,
    const SerializeOptions& options)
{
    // Prefer echoing the preserved source snapshot. This is the Phase 2
    // round-trip guarantee: an unmutated load→serialize is byte-identical.
    if (!options.force_reconstruct && !scene.mutated &&
        !scene.preserved_source.empty()) {
        return Result<std::string>::ok(scene.preserved_source);
    }

    // Otherwise reconstruct programmatically from SceneData + unknowns.
    std::string xml = reconstruct_scene_xml(scene, options);
    return Result<std::string>::ok(std::move(xml));
}

Result<bool> serialize_scene(
    const SceneData& scene,
    const std::filesystem::path& path,
    const SerializeOptions& options)
{
    auto s = serialize_scene_to_string(scene, options);
    if (s.is_err()) return Result<bool>::err(s.error());

    // Ensure parent directory exists.
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return Result<bool>::err(
                "Failed to create scene parent dir: " + parent.string() +
                " (" + ec.message() + ")");
        }
    }

    // Write in BINARY mode so CRLF is not synthesized — paired with the
    // loader's binary read, this makes byte-identical round-trip work on
    // Windows without line-ending drift.
    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return Result<bool>::err(
            "Failed to open scene for write: " + path.string());
    }
    const std::string& buf = std::move(s).value();
    out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    if (!out.good()) {
        return Result<bool>::err(
            "Failed to write scene: " + path.string());
    }
    spdlog::info("Serialized scene to '{}' ({} bytes)",
                 path.string(), buf.size());
    return Result<bool>::ok(true);
}

} // namespace odyssey::scene
