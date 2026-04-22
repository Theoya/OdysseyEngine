#include "assets/lighting_profile_loader.h"

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <sstream>
#include <cstring>

namespace odyssey::assets {

// ---------------------------------------------------------------------------
// Internal helpers (pure, file-local)
// ---------------------------------------------------------------------------

// Parse "r g b" triplet from a space-separated attribute value.
// Returns defaults on any parse failure — missing components keep their default.
static void parse_rgb_triplet(const char* str,
                               float& out_r, float& out_g, float& out_b,
                               float default_r, float default_g, float default_b) {
    if (!str || str[0] == '\0') {
        out_r = default_r;
        out_g = default_g;
        out_b = default_b;
        return;
    }
    // Use sscanf: if fewer than 3 fields parse, the remainder keep defaults.
    out_r = default_r; out_g = default_g; out_b = default_b;
    std::sscanf(str, "%f %f %f", &out_r, &out_g, &out_b);
}

// ---------------------------------------------------------------------------
// parse_lighting_profile_xml — PURE
// ---------------------------------------------------------------------------
Result<LightingProfileData, LightingProfileError>
parse_lighting_profile_xml(const std::string& xml_content) {
    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_string(xml_content.c_str());
    if (!parse_result) {
        spdlog::warn("lighting_profile_loader: XML parse error — {}",
                     parse_result.description());
        return Result<LightingProfileData, LightingProfileError>::err(
            LightingProfileError::XmlParseError);
    }

    auto root = doc.child("lighting_profile");
    if (!root) {
        spdlog::warn("lighting_profile_loader: missing <lighting_profile> root");
        return Result<LightingProfileData, LightingProfileError>::err(
            LightingProfileError::MissingRoot);
    }

    const char* preset_attr = root.attribute("preset").as_string(nullptr);
    if (!preset_attr || preset_attr[0] == '\0') {
        spdlog::warn("lighting_profile_loader: missing preset attribute");
        return Result<LightingProfileData, LightingProfileError>::err(
            LightingProfileError::MissingPreset);
    }

    LightingProfileData data;
    data.preset  = preset_attr;
    data.version = root.attribute("version").as_int(1);

    // --- <post_fx> block ---
    auto post_fx = root.child("post_fx");
    if (post_fx) {
        // <tonemap curve="aces" exposure="1.00"/>
        auto tonemap = post_fx.child("tonemap");
        if (tonemap) {
            data.tonemap_exposure = tonemap.attribute("exposure").as_float(1.0f);
        }

        // <bloom threshold="..." intensity="..." radius="..."/>
        auto bloom = post_fx.child("bloom");
        if (bloom) {
            data.bloom_threshold = bloom.attribute("threshold").as_float(1.5f);
            data.bloom_intensity = bloom.attribute("intensity").as_float(0.35f);
            data.bloom_radius    = bloom.attribute("radius").as_float(0.30f);
        }

        // <grade lift="r g b" gamma="r g b" gain="r g b"/>
        auto grade = post_fx.child("grade");
        if (grade) {
            parse_rgb_triplet(grade.attribute("lift").as_string(""),
                data.grade_lift_r,  data.grade_lift_g,  data.grade_lift_b,
                0.f, 0.f, 0.f);
            parse_rgb_triplet(grade.attribute("gamma").as_string(""),
                data.grade_gamma_r, data.grade_gamma_g, data.grade_gamma_b,
                1.f, 1.f, 1.f);
            parse_rgb_triplet(grade.attribute("gain").as_string(""),
                data.grade_gain_r,  data.grade_gain_g,  data.grade_gain_b,
                1.f, 1.f, 1.f);
        }

        // <vignette strength="..." roundness="..."/>
        auto vignette = post_fx.child("vignette");
        if (vignette) {
            data.vignette_strength  = vignette.attribute("strength").as_float(0.8f);
            data.vignette_roundness = vignette.attribute("roundness").as_float(1.0f);
        }

        // <grain intensity="..." response="..."/>
        auto grain = post_fx.child("grain");
        if (grain) {
            data.grain_intensity = grain.attribute("intensity").as_float(0.03f);
            data.grain_response  = grain.attribute("response").as_float(0.7f);
        }

        // <chromatic_aberration strength="..."/> — optional element
        auto chrom = post_fx.child("chromatic_aberration");
        if (chrom) {
            data.chromatic_aberration     = chrom.attribute("strength").as_float(0.0f);
            data.has_chromatic_aberration = true;
        }
    }

    spdlog::debug("lighting_profile_loader: parsed preset '{}' v{}",
                  data.preset, data.version);
    return Result<LightingProfileData, LightingProfileError>::ok(std::move(data));
}

// ---------------------------------------------------------------------------
// load_lighting_profile_file — IMPURE
// ---------------------------------------------------------------------------
Result<LightingProfileData, LightingProfileError>
load_lighting_profile_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        spdlog::warn("lighting_profile_loader: file not found: {}",
                     path.string());
        return Result<LightingProfileData, LightingProfileError>::err(
            LightingProfileError::FileNotFound);
    }

    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        spdlog::warn("lighting_profile_loader: could not open: {}",
                     path.string());
        return Result<LightingProfileData, LightingProfileError>::err(
            LightingProfileError::FileOpenFailed);
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    spdlog::info("lighting_profile_loader: loading '{}'", path.string());
    return parse_lighting_profile_xml(ss.str());
}

// ---------------------------------------------------------------------------
// profile_to_crt_params — PURE
//
// Overlay the profile's photographic intent on top of base_crt.
// See header derivation comment for the exposure→brightness mapping.
// ---------------------------------------------------------------------------
vulkan::CRTParams profile_to_crt_params(const vulkan::CRTParams& base_crt,
                                         const LightingProfileData& profile) noexcept {
    vulkan::CRTParams out = base_crt;

    // exposure → brightness: linear scale of the existing base brightness.
    // base_crt.brightness already incorporates the HUDParams::brightness_boost;
    // we scale the whole value so the profile acts like a global exposure
    // correction on top of whatever the game sets.
    out.brightness = base_crt.brightness * profile.tonemap_exposure;

    // vignette_strength: profile value replaces the engine default entirely.
    // Rationale: vignette is a compositional choice authored per-zone;
    // the game's HUD boost (from HUDParams::vignette_boost) was already
    // folded into base_crt before this call, so we replace, not add.
    out.vignette_strength = profile.vignette_strength;

    // grain_intensity → flicker_amount: direct copy.
    // See header for the grain/flicker analogy reasoning.
    out.flicker_amount = profile.grain_intensity;

    // chromatic_aberration: only override when the profile element was present.
    // Profiles that omit the element (e.g. Warmth) leave the CRT default alone.
    if (profile.has_chromatic_aberration) {
        out.chromatic_aberration = profile.chromatic_aberration;
    }

    return out;
}

// ---------------------------------------------------------------------------
// profile_to_eva_params — PURE: no current mapping; stable extension point.
// ---------------------------------------------------------------------------
vulkan::EvaHUDParams profile_to_eva_params(const vulkan::EvaHUDParams& base_eva,
                                            const LightingProfileData& /*profile*/) noexcept {
    return base_eva;
}

// ---------------------------------------------------------------------------
// resolve_lighting_profile_path — PURE
//
// Returns the ordered list of candidate filesystem paths for a named profile.
// Derivation: scene-local overrides take priority over the shared showcase
// fallback, giving per-game scenes the ability to shadow global profiles
// without touching the engine or the showcase directory.
// ---------------------------------------------------------------------------
std::vector<std::filesystem::path>
resolve_lighting_profile_path(const std::string& name,
                               const std::filesystem::path& scene_dir) {
    const std::string filename = name + ".xml";
    return {
        scene_dir / "lighting_profiles" / filename,
        std::filesystem::path("demo/showcase/lighting_profiles") / filename,
    };
}

} // namespace odyssey::assets
