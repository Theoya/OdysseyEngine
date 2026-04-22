#pragma once
//
// lighting_profile_loader.h — pure parser for .lighting_profile.xml files.
//
// A lighting profile describes the photographic + post-FX intent for a scene
// zone. Six profiles are authored under demo/showcase/lighting_profiles/:
//   dread, hostile, liminal, sacred, warmth, wonder.
//
// The profile is referenced in scene XML as an attribute on the root element:
//   <scene name="showcase" ... lighting_profile="liminal">
//
// The scene loader captures this as an unknown_scene_attribute; the engine
// resolves it to a file path and calls parse_lighting_profile_xml() at
// scene-load time.
//
// IMPORTANT: this loader is deliberately separate from the Vulkan renderer.
// LightingProfileData is a plain data struct — the mapping to CRTParams /
// EvaHUDParams lives in profile_to_crt_params() / profile_to_eva_params()
// below, so the pure data layer has no Vulkan dependency and can be unit-
// tested without a GPU.
//
// Mandate compliance:
//   - parse_lighting_profile_xml is pure (string → Result<LightingProfileData>).
//   - load_lighting_profile_file is impure (filesystem read).
//   - profile_to_crt_params / profile_to_eva_params are pure (data → data).
//   - Every Result<T,E>-returning function has success + failure tests in
//     tests/unit/assets/test_lighting_profile.cpp.
//

#include "core/result.h"
#include "vulkan/postprocess.h"  // CRTParams, EvaHUDParams

#include <string>
#include <filesystem>

namespace odyssey::assets {

// ---------------------------------------------------------------------------
// LightingProfileData — parsed representation of one .lighting_profile.xml.
//
// Only the fields that map to the current renderer are preserved; the rest
// (palette kelvin, disallowed_sources, volumetrics, directional_override) are
// read and validated but not stored — they require future subsystems (a
// dedicated light buffer, volumetric compute) before they can be acted upon.
// Adding those fields would be a UBO/SSBO layout change and is council-gated.
// ---------------------------------------------------------------------------
struct LightingProfileData {
    // Metadata
    std::string preset;    // e.g. "Warmth", "Dread"
    int         version = 1;

    // --- post_fx fields (all map to existing CRTParams / EvaHUDParams) ---

    // <tonemap curve="aces" exposure="1.00"/>
    // Exposure scales CRTParams::brightness (base 1.2 per engine.cpp).
    // Derivation: brightness_out = base_brightness * exposure
    //   — exposure=1.0 → no change; exposure=0.75 (Dread) → dimmer scene.
    float tonemap_exposure = 1.0f;

    // <bloom threshold="..." intensity="..." radius="..."/>
    // Preserved for future bloom pass; not currently mapped.
    float bloom_threshold = 1.5f;
    float bloom_intensity = 0.35f;
    float bloom_radius    = 0.30f;

    // <grade lift="r g b" gamma="r g b" gain="r g b"/>
    // Preserved for future color-grade pass; not currently mapped.
    // (Packed as 3 vec3s for completeness.)
    float grade_lift_r = 0.f,  grade_lift_g = 0.f,  grade_lift_b = 0.f;
    float grade_gamma_r = 1.f, grade_gamma_g = 1.f, grade_gamma_b = 1.f;
    float grade_gain_r = 1.f,  grade_gain_g = 1.f,  grade_gain_b = 1.f;

    // <vignette strength="..." roundness="..."/>
    // Maps directly to CRTParams::vignette_strength.
    float vignette_strength  = 0.8f;
    float vignette_roundness = 1.0f;

    // <grain intensity="..." response="..."/>
    // intensity maps to CRTParams::flicker_amount (closest available knob).
    // Derivation: grain is a high-frequency noise; flicker_amount is the
    // closest analogue in the CRT shader's noise budget. They are not
    // identical but serve the same "add perceptual texture" role.
    float grain_intensity = 0.03f;
    float grain_response  = 0.7f;

    // <chromatic_aberration strength="..."/> (optional — absent = 0)
    // Maps to CRTParams::chromatic_aberration.
    float chromatic_aberration = 0.0f;

    // has_chromatic_aberration: true only when the element was present in XML.
    // When false, the engine does not override the CRT shader's default.
    bool has_chromatic_aberration = false;
};

// ---------------------------------------------------------------------------
// LightingProfileError — distinct error modes for success+failure test coverage.
// ---------------------------------------------------------------------------
enum class LightingProfileError : uint32_t {
    XmlParseError   = 1,   // pugixml could not parse the string
    MissingRoot     = 2,   // no <lighting_profile> root element
    MissingPreset   = 3,   // preset attribute absent or empty
    FileNotFound    = 4,   // load_lighting_profile_file: file missing
    FileOpenFailed  = 5,   // load_lighting_profile_file: open error
};

// ---------------------------------------------------------------------------
// parse_lighting_profile_xml — PURE: parses a .lighting_profile.xml string.
//
// Only the <post_fx> block affects current rendering; other blocks are
// validated for presence but not stored (see struct comment above).
// ---------------------------------------------------------------------------
Result<LightingProfileData, LightingProfileError>
parse_lighting_profile_xml(const std::string& xml_content);

// ---------------------------------------------------------------------------
// load_lighting_profile_file — IMPURE: reads a file, then calls the pure parser.
// ---------------------------------------------------------------------------
Result<LightingProfileData, LightingProfileError>
load_lighting_profile_file(const std::filesystem::path& path);

// ---------------------------------------------------------------------------
// profile_to_crt_params — PURE: produce a CRTParams from a profile.
//
// The base_crt argument is the per-frame default (e.g. from HUDParams boosts).
// This function overlays the profile's photographic intent on top.
//
// Mapping table:
//   profile.tonemap_exposure      → crt.brightness  = base_crt.brightness
//                                                     * tonemap_exposure
//   profile.vignette_strength     → crt.vignette_strength = vignette_strength
//   profile.grain_intensity       → crt.flicker_amount   = grain_intensity
//   profile.chromatic_aberration  → crt.chromatic_aberration (only if present)
//   everything else               → passes through from base_crt unchanged
//
// Derivation of exposure→brightness:
//   Physical cameras express exposure in EV stops: EV = log2(brightness).
//   The CRT shader's `brightness` is a linear multiplier on the colour.
//   Multiplying the base brightness by the profile's exposure value produces
//   the correct linear-space scaling: at exposure=1.0 there is no change;
//   at exposure=0.75 (Dread) the scene is 25% dimmer; at exposure=1.2 (rare)
//   it is 20% brighter. This is an approximation — a full EV stop would be
//   base * 2^(exposure-1) — but the profiles are authored in the [0.75, 1.25]
//   range where linear and EV-stop multiplication are within 10% of each
//   other, and the authors intended a linear multiplier per the XML comments.
// ---------------------------------------------------------------------------
vulkan::CRTParams profile_to_crt_params(const vulkan::CRTParams& base_crt,
                                         const LightingProfileData& profile) noexcept;

// ---------------------------------------------------------------------------
// profile_to_eva_params — PURE: no current mapping exists between lighting
// profile fields and EvaHUDParams. Returns base_eva unchanged. Provided as
// a stable extension point so callers don't special-case this frame and
// don't need to change call sites when a mapping is added later.
// ---------------------------------------------------------------------------
vulkan::EvaHUDParams profile_to_eva_params(const vulkan::EvaHUDParams& base_eva,
                                            const LightingProfileData& profile) noexcept;

// ---------------------------------------------------------------------------
// resolve_lighting_profile_path — PURE
//
// Given a profile name (e.g. "liminal") and the directory that contains the
// scene file, returns the ordered candidate paths to search.
//
// Resolution order:
//   1. <scene_dir>/lighting_profiles/<name>.xml
//   2. demo/showcase/lighting_profiles/<name>.xml  (engine-wide fallback)
//
// The caller is responsible for checking which path exists and calling
// load_lighting_profile_file on the first hit.  Returning a vector (rather
// than immediately testing existence) keeps this function pure — no
// filesystem side-effects.
// ---------------------------------------------------------------------------
std::vector<std::filesystem::path>
resolve_lighting_profile_path(const std::string& name,
                               const std::filesystem::path& scene_dir);

} // namespace odyssey::assets
