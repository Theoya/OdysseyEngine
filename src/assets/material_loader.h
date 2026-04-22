#pragma once
#include "core/types.h"
#include "core/result.h"
#include "vulkan/bindless_texture_registry.h"
#include <cstdint>
#include <string>
#include <filesystem>

namespace odyssey::assets {

/// ---------------------------------------------------------------------------
/// MaterialData — the parsed, CPU-side material descriptor.
/// Loaded from .mat.xml (schema UNCHANGED — no new fields serialized).
/// The 32-bit texture index is a runtime-only field, never written to disk.
/// ---------------------------------------------------------------------------
struct MaterialData {
    std::string name;
    int version = 1;

    // Shader paths
    std::string vertex_shader;
    std::string fragment_shader;

    // Base color (Lambertian diffuse albedo).  Not PBR — Mandate 4.
    vec4 albedo{1.0f};

    // Texture slots parsed from <textures> element.
    // Each slot maps to a path; resolve_material_gpu() converts to indices.
    // We intentionally do NOT name these metallic/roughness to avoid PBR drift
    // (vibe-story-guardian condition).
    std::string albedo_map;     // <textures><albedo src="..."/>

    // NOTE: normal_map, metallic_map, roughness_map are parsed for
    // round-trip fidelity but NOT promoted to bindless indices in Phase 6.
    // Adding a PBR-coded path re-triggers council (vibe condition).
    std::string normal_map;
    std::string metallic_map;
    std::string roughness_map;
};

/// ---------------------------------------------------------------------------
/// MaterialGPU — std430 struct for the material index buffer.
///
/// std430 derivation (M3 first-principles):
///   - Each member is a scalar or vec4; vec4 = 4×4 = 16 bytes, aligned to 16.
///   - uint32_t members are 4 bytes, aligned to 4.
///   - No implicit padding needed between these types when laid out as shown.
///
/// Byte offsets (std430):
///   offset 0:  albedo_r    (float, 4 bytes)
///   offset 4:  albedo_g    (float, 4 bytes)
///   offset 8:  albedo_b    (float, 4 bytes)
///   offset 12: albedo_a    (float, 4 bytes)
///   ── 16 bytes so far ──
///   offset 16: albedo_tex_index  (uint32_t, 4 bytes)
///              0 = sentinel (no texture, use albedo color)
///              non-zero = TextureHandle::slot() in the bindless array
///   offset 20: _pad0       (uint32_t, 4 bytes — padding to 8-byte align)
///   offset 24: _pad1       (uint32_t, 4 bytes)
///   offset 28: _pad2       (uint32_t, 4 bytes)
///   ── 32 bytes total ──
///
/// The 32-bit index packing: albedo_tex_index directly holds the slot index
/// (lower 16 bits of a TextureHandle).  Generation counter is NOT stored in
/// the GPU buffer — it's a CPU-only staleness guard.  Slot 0 is always the
/// magenta sentinel, so 0 means "use albedo color" (no sampler needed).
/// ---------------------------------------------------------------------------
struct MaterialGPU {
    float    albedo_r        = 1.0f; // offset 0
    float    albedo_g        = 1.0f; // offset 4
    float    albedo_b        = 1.0f; // offset 8
    float    albedo_a        = 1.0f; // offset 12
    uint32_t albedo_tex_index = 0u;  // offset 16: bindless slot (0 = sentinel/no-texture)
    uint32_t _pad0           = 0u;   // offset 20
    uint32_t _pad1           = 0u;   // offset 24
    uint32_t _pad2           = 0u;   // offset 28
};

// Compile-time layout verification (M2/M3 mandate).
static_assert(sizeof(MaterialGPU) == 32,
    "MaterialGPU must be 32 bytes for std430 alignment");
static_assert(offsetof(MaterialGPU, albedo_r)         ==  0);
static_assert(offsetof(MaterialGPU, albedo_tex_index) == 16);

/// Error codes for material resolution.
enum class MaterialResolveErr : uint32_t {
    TextureLoadFailed,    ///< referenced texture path could not be loaded
    RegistryFull,         ///< bindless slot table exhausted
    IndexOutOfRange,      ///< resolved slot >= MAX_BINDLESS_TEXTURES
};

std::string material_resolve_err_to_string(MaterialResolveErr e);

// Pure: parse material XML (schema unchanged — round-trip byte-identical).
Result<MaterialData> parse_material_xml(const std::string& xml_content);

// Impure: load from file.
Result<MaterialData> load_material_file(const std::filesystem::path& path);

// Impure: resolve MaterialData to MaterialGPU.
// Loads albedo_map texture into the registry if not already loaded.
// Returns MaterialGPU with albedo_tex_index set to the bindless slot.
// tex_load_rgba: caller-supplied loader: path → (pixels, w, h) or error.
// This indirection keeps material_loader pure-testable.
Result<MaterialGPU, MaterialResolveErr> resolve_material_gpu(
    const MaterialData& material,
    vulkan::BindlessTextureRegistry& registry,
    VkCommandPool command_pool,
    // texture loader callback: returns (width, height, rgba_data) or empty on fail
    std::function<std::vector<uint8_t>(const std::filesystem::path&, uint32_t&, uint32_t&)> tex_load
);

} // namespace odyssey::assets
