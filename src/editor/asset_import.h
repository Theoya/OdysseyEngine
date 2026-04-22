#pragma once

// ---------------------------------------------------------------------------
// asset_import.h
// Editor asset import pipeline: File → Import Asset, drag-drop, etc.
//
// Responsibilities:
//   - Classify source files (.obj, .png, .wav, .glb, .fbx, .xml).
//   - Plan import destinations based on project root and source type.
//   - Generate .mesh.xml descriptors wrapping .obj files.
//   - Execute the full import: copy files, write descriptors, validate XSD.
//
// Pure functions:
//   - plan_import(source, project_root) -> ImportTarget
//   - generate_mesh_xml_for_obj(obj_path) -> string (XML text)
//
// Impure wrappers:
//   - execute_import(source, project_root, overwrite=false) -> Result
// ---------------------------------------------------------------------------

#include "core/result.h"
#include "editor/asset_browser_panel.h"

#include <filesystem>
#include <optional>
#include <string>

namespace odyssey::editor {

// Source file being imported.
struct ImportSource {
    std::filesystem::path source_abs;  // Absolute path to source file
};

// Target locations + metadata for an import operation.
struct ImportTarget {
    std::filesystem::path target_abs;               // Main copied file
    std::optional<std::filesystem::path> descriptor_abs;  // .mesh.xml etc., if generated
    AssetType type = AssetType::Other;
};

// ---------------------------------------------------------------------------
// Pure logic
// ---------------------------------------------------------------------------

// Classify a source file extension to determine its import strategy.
// Returns Err if the extension is unsupported.
Result<AssetType, std::string> classify_import_source(
    const std::filesystem::path& source);

// Plan where a source file should go given a project root.
// Does NOT touch the filesystem; purely determines target paths.
// Returns Err if source extension is unsupported or source path is empty.
Result<ImportTarget, std::string> plan_import(
    const ImportSource& source, const std::filesystem::path& project_root);

// Generate XML text for a .mesh.xml descriptor wrapping an .obj file.
// Pure: no I/O. Takes the path relative to project root (as written in descriptor).
// Returns valid XML that matches mesh.xsd. Caller writes the file.
std::string generate_mesh_xml_for_obj(
    const std::filesystem::path& obj_relative_to_project);

// ---------------------------------------------------------------------------
// Impure I/O wrappers
// ---------------------------------------------------------------------------

// Open a file dialog for importing assets (all extensions).
// Returns the selected file path, or nullopt if cancelled.
std::optional<std::filesystem::path> open_import_dialog();

// Execute the full import: validate source, plan destination, copy file,
// write descriptor if needed. Creates directories as needed.
// If target file exists and overwrite=false, returns Err.
// On success, returns the ImportTarget with absolute paths set.
Result<ImportTarget, std::string> execute_import(
    const ImportSource& source, const std::filesystem::path& project_root,
    bool overwrite = false);

} // namespace odyssey::editor
