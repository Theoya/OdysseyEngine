#include "editor/asset_import.h"

#include "editor/file_dialog_win32.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace odyssey::editor {

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

static std::string lowercase(const std::string& s) {
    std::string result = s;
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }
    return result;
}

// Returns the file extension (without the dot), lowercase.
// E.g., "file.obj" -> "obj", "file.mesh.xml" -> "xml"
static std::string get_extension(const std::filesystem::path& p) {
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    return lowercase(ext);
}

// Check for double extensions (e.g., "crate.mesh.xml" -> ".mesh.xml" -> "mesh.xml")
static std::string get_double_extension(const std::filesystem::path& p) {
    std::string stem_ext = p.stem().extension().string();
    if (!stem_ext.empty() && stem_ext[0] == '.') {
        stem_ext = stem_ext.substr(1);
    }
    stem_ext += ".";
    stem_ext += get_extension(p);
    return lowercase(stem_ext);
}

// Get the "base" name without any extension (for target path generation).
static std::filesystem::path get_base_name(const std::filesystem::path& p) {
    std::string filename = p.filename().string();

    // Check double extensions first
    auto stem = p.stem().string();
    auto stem_ext = p.stem().extension().string();
    if (!stem_ext.empty()) {
        stem_ext = lowercase(stem_ext);
        if (stem_ext == ".mesh" || stem_ext == ".mat" || stem_ext == ".anim" ||
            stem_ext == ".prefab" || stem_ext == ".scene" || stem_ext == ".skeleton" ||
            stem_ext == ".actions" || stem_ext == ".music") {
            // Double extension found, base is the stem of the stem
            return std::filesystem::path(p.stem().stem().string());
        }
    }
    return p.stem();
}

// ---------------------------------------------------------------------------
// Public pure functions
// ---------------------------------------------------------------------------

Result<AssetType, std::string> classify_import_source(
    const std::filesystem::path& source) {
    std::string ext = get_extension(source);

    if (ext == "obj") return Result<AssetType, std::string>::ok(AssetType::Mesh);
    if (ext == "png") return Result<AssetType, std::string>::ok(AssetType::Other);  // Generic "image"
    if (ext == "jpg" || ext == "jpeg") return Result<AssetType, std::string>::ok(AssetType::Other);
    if (ext == "wav") return Result<AssetType, std::string>::ok(AssetType::Other);  // Generic "audio"
    if (ext == "glb") return Result<AssetType, std::string>::ok(AssetType::Other);  // "gltf"
    if (ext == "fbx") return Result<AssetType, std::string>::ok(AssetType::Other);  // "fbx"
    if (ext == "xml") return Result<AssetType, std::string>::ok(AssetType::Other);  // "xml"

    return Result<AssetType, std::string>::err(
        std::string("unsupported extension: .") + ext);
}

Result<ImportTarget, std::string> plan_import(
    const ImportSource& source, const std::filesystem::path& project_root) {

    if (source.source_abs.empty()) {
        return Result<ImportTarget, std::string>::err("source path is empty");
    }

    auto ext = get_extension(source.source_abs);
    auto base_name = get_base_name(source.source_abs);

    ImportTarget target;
    target.type = AssetType::Other;

    // Determine destination based on extension
    if (ext == "obj") {
        // .obj -> meshes/ with .mesh.xml descriptor
        target.target_abs = project_root / "meshes" / (base_name.string() + ".obj");
        target.descriptor_abs = project_root / "meshes" / (base_name.string() + ".mesh.xml");
        target.type = AssetType::Mesh;
    } else if (ext == "png") {
        target.target_abs = project_root / "textures" / (base_name.string() + ".png");
        target.type = AssetType::Other;
    } else if (ext == "jpg" || ext == "jpeg") {
        target.target_abs = project_root / "textures" / (base_name.string() + "." + ext);
        target.type = AssetType::Other;
    } else if (ext == "wav") {
        target.target_abs = project_root / "audio" / (base_name.string() + ".wav");
        target.type = AssetType::Other;
    } else if (ext == "glb") {
        target.target_abs = project_root / "imported" / (base_name.string() + ".glb");
        target.type = AssetType::Other;
    } else if (ext == "fbx") {
        target.target_abs = project_root / "imported" / (base_name.string() + ".fbx");
        target.type = AssetType::Other;
    } else if (ext == "xml") {
        // Classify by double extension or XML content (simplified: assume "Other" for now)
        // Could do more sophisticated classification here, but for now: imported/
        target.target_abs = project_root / "imported" / source.source_abs.filename();
        target.type = AssetType::Other;
    } else {
        return Result<ImportTarget, std::string>::err(
            std::string("unsupported extension: .") + ext);
    }

    return Result<ImportTarget, std::string>::ok(target);
}

std::string generate_mesh_xml_for_obj(
    const std::filesystem::path& obj_relative_to_project) {
    // Generate a minimal valid mesh.xml wrapping an .obj file.
    // obj_relative_to_project should be something like "meshes/mycrate.obj"

    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<!-- Auto-generated mesh descriptor from .obj import -->\n";
    oss << "<mesh name=\"" << obj_relative_to_project.stem().string() << "\" version=\"1\">\n";
    oss << "  <source format=\"obj\" path=\"" << obj_relative_to_project.string() << "\"/>\n";
    oss << "  <lod>\n";
    oss << "    <level distance=\"0\" triangles=\"0\"/>\n";
    oss << "  </lod>\n";
    oss << "  <collider type=\"box\" auto_fit=\"true\"/>\n";
    oss << "</mesh>\n";

    return oss.str();
}

// ---------------------------------------------------------------------------
// Impure I/O wrappers
// ---------------------------------------------------------------------------

std::optional<std::filesystem::path> open_import_dialog() {
    // Native Win32 dialog accepting all file types
    // We'll open with a generic filter for all files

    // This is a placeholder that calls a Win32 dialog
    // For now, we'll use a similar pattern to the scene dialog
    // but without the .scene.xml filter

    // Implementation deferred to file_dialog_win32.cpp if needed,
    // or we can inline Win32 code here. For MVP, return nullopt.
    return std::nullopt;
}

Result<ImportTarget, std::string> execute_import(
    const ImportSource& source, const std::filesystem::path& project_root,
    bool overwrite) {

    // Validate source exists
    if (!std::filesystem::exists(source.source_abs)) {
        return Result<ImportTarget, std::string>::err(
            std::string("source file not found: ") + source.source_abs.string());
    }

    // Plan the import
    auto plan_result = plan_import(source, project_root);
    if (!plan_result.is_ok()) {
        return Result<ImportTarget, std::string>::err(plan_result.error());
    }

    ImportTarget target = plan_result.value();

    // Check if target exists and !overwrite
    if (std::filesystem::exists(target.target_abs) && !overwrite) {
        return Result<ImportTarget, std::string>::err(
            std::string("target file already exists: ") + target.target_abs.string());
    }

    // Create parent directories if needed
    try {
        auto parent = target.target_abs.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        return Result<ImportTarget, std::string>::err(
            std::string("failed to create directory: ") + e.what());
    }

    // Copy source to target
    try {
        std::filesystem::copy(
            source.source_abs,
            target.target_abs,
            std::filesystem::copy_options::overwrite_existing);
    } catch (const std::filesystem::filesystem_error& e) {
        return Result<ImportTarget, std::string>::err(
            std::string("failed to copy file: ") + e.what());
    }

    // Generate and write descriptor if needed
    if (target.descriptor_abs) {
        std::string desc_xml = generate_mesh_xml_for_obj(
            std::filesystem::relative(target.target_abs, project_root));

        try {
            std::ofstream out(target.descriptor_abs.value());
            if (!out) {
                return Result<ImportTarget, std::string>::err(
                    std::string("failed to open descriptor for writing: ") +
                    target.descriptor_abs.value().string());
            }
            out << desc_xml;
            out.close();
        } catch (const std::exception& e) {
            return Result<ImportTarget, std::string>::err(
                std::string("failed to write descriptor: ") + e.what());
        }
    }

    spdlog::info("[import] imported {} to {}", source.source_abs.string(),
                 target.target_abs.string());
    if (target.descriptor_abs) {
        spdlog::info("[import] created descriptor at {}",
                     target.descriptor_abs->string());
    }

    return Result<ImportTarget, std::string>::ok(target);
}

} // namespace odyssey::editor
