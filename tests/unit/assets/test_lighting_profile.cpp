//
// test_lighting_profile.cpp — unit tests for lighting_profile_loader.
//
// Mandate: every Result<T,E>-returning function must have at least one
// expected-success test per success path and one expected-failure test per
// distinct error mode (LightingProfileError has 5 modes).
//
// parse_lighting_profile_xml: pure — tested here exhaustively.
// load_lighting_profile_file: impure (file IO) — smoke-tested against a
//   real file from demo/showcase/lighting_profiles/ (copied to build dir by
//   the copy_demo_assets CMake target).
// profile_to_crt_params: pure — tested with known inputs/outputs.
// profile_to_eva_params: pure — passthrough verified.
//

#include "assets/lighting_profile_loader.h"
#include "vulkan/postprocess.h"

#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>

using namespace odyssey;
using namespace odyssey::assets;
using namespace odyssey::vulkan;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Minimal valid XML for use in positive tests.
static constexpr const char* kMinimalXml = R"xml(
<?xml version="1.0" encoding="UTF-8"?>
<lighting_profile preset="TestZone" version="1">
  <palette>
    <primary_kelvin min="3800" max="4400"/>
  </palette>
  <fog type="exponential" density="0.008" color_kelvin="4000"/>
  <volumetrics enabled="0" god_rays="0" density_variance="0" steps="0"/>
  <directional_override kelvin="6500" intensity="0.3" direction="0 -1 0"/>
  <post_fx>
    <tonemap  curve="aces" exposure="1.10"/>
    <bloom    threshold="1.8" intensity="0.25" radius="0.25"/>
    <grade    lift="0.00 0.02 0.00" gamma="1.00 1.00 1.00" gain="0.96 1.02 0.98"/>
    <vignette strength="0.06" roundness="1.5"/>
    <grain    intensity="0.05" response="0.6"/>
  </post_fx>
  <disallowed_sources/>
</lighting_profile>
)xml";

// Like kMinimalXml but with an explicit <chromatic_aberration> element.
static constexpr const char* kWithChromXml = R"xml(
<?xml version="1.0" encoding="UTF-8"?>
<lighting_profile preset="Dread" version="2">
  <post_fx>
    <tonemap  curve="aces" exposure="0.75"/>
    <vignette strength="0.35" roundness="0.9"/>
    <grain    intensity="0.07" response="0.9"/>
    <chromatic_aberration strength="0.0020"/>
  </post_fx>
</lighting_profile>
)xml";

// ---------------------------------------------------------------------------
// parse_lighting_profile_xml — success paths
// ---------------------------------------------------------------------------

TEST(LightingProfileParser, SuccessMinimalProfile) {
    auto result = parse_lighting_profile_xml(kMinimalXml);
    ASSERT_TRUE(result.is_ok()) << "Expected parse to succeed";
    const auto& d = result.value();
    EXPECT_EQ(d.preset, "TestZone");
    EXPECT_EQ(d.version, 1);
    EXPECT_NEAR(d.tonemap_exposure,    1.10f, 1e-4f);
    EXPECT_NEAR(d.bloom_threshold,     1.80f, 1e-4f);
    EXPECT_NEAR(d.bloom_intensity,     0.25f, 1e-4f);
    EXPECT_NEAR(d.vignette_strength,   0.06f, 1e-4f);
    EXPECT_NEAR(d.vignette_roundness,  1.50f, 1e-4f);
    EXPECT_NEAR(d.grain_intensity,     0.05f, 1e-4f);
    EXPECT_NEAR(d.grain_response,      0.60f, 1e-4f);
    EXPECT_FALSE(d.has_chromatic_aberration);
    EXPECT_NEAR(d.chromatic_aberration, 0.0f, 1e-6f);
}

TEST(LightingProfileParser, SuccessProfileWithChromaticAberration) {
    auto result = parse_lighting_profile_xml(kWithChromXml);
    ASSERT_TRUE(result.is_ok()) << "Expected parse with chromatic_aberration to succeed";
    const auto& d = result.value();
    EXPECT_EQ(d.preset, "Dread");
    EXPECT_EQ(d.version, 2);
    EXPECT_NEAR(d.tonemap_exposure,     0.75f,   1e-4f);
    EXPECT_NEAR(d.vignette_strength,    0.35f,   1e-4f);
    EXPECT_NEAR(d.grain_intensity,      0.07f,   1e-4f);
    EXPECT_TRUE(d.has_chromatic_aberration);
    EXPECT_NEAR(d.chromatic_aberration, 0.0020f, 1e-6f);
}

TEST(LightingProfileParser, SuccessGradeComponentsParsed) {
    auto result = parse_lighting_profile_xml(kMinimalXml);
    ASSERT_TRUE(result.is_ok());
    const auto& d = result.value();
    EXPECT_NEAR(d.grade_lift_r,  0.00f, 1e-4f);
    EXPECT_NEAR(d.grade_lift_g,  0.02f, 1e-4f);
    EXPECT_NEAR(d.grade_lift_b,  0.00f, 1e-4f);
    EXPECT_NEAR(d.grade_gain_r,  0.96f, 1e-4f);
    EXPECT_NEAR(d.grade_gain_g,  1.02f, 1e-4f);
    EXPECT_NEAR(d.grade_gain_b,  0.98f, 1e-4f);
}

TEST(LightingProfileParser, SuccessDefaultsWhenPostFxAbsent) {
    // Profile with no <post_fx> block — all post-fx fields should get defaults.
    static constexpr const char* kNoPostFx = R"xml(
<lighting_profile preset="Bare" version="1">
  <palette><primary_kelvin min="3000" max="4000"/></palette>
</lighting_profile>
)xml";
    auto result = parse_lighting_profile_xml(kNoPostFx);
    ASSERT_TRUE(result.is_ok());
    const auto& d = result.value();
    EXPECT_EQ(d.preset, "Bare");
    EXPECT_NEAR(d.tonemap_exposure,  1.0f, 1e-4f);  // default
    EXPECT_NEAR(d.vignette_strength, 0.8f, 1e-4f);  // default
    EXPECT_NEAR(d.grain_intensity,   0.03f, 1e-4f); // default
    EXPECT_FALSE(d.has_chromatic_aberration);
}

// ---------------------------------------------------------------------------
// parse_lighting_profile_xml — failure paths (one per LightingProfileError mode)
// ---------------------------------------------------------------------------

TEST(LightingProfileParser, FailMalformedXml) {
    auto result = parse_lighting_profile_xml("<<not xml at all>>");
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), LightingProfileError::XmlParseError);
}

TEST(LightingProfileParser, FailMissingRoot) {
    auto result = parse_lighting_profile_xml(
        "<some_other_root preset=\"X\"/>");
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), LightingProfileError::MissingRoot);
}

TEST(LightingProfileParser, FailMissingPresetAttribute) {
    static constexpr const char* kNoPreset = R"xml(
<lighting_profile version="1">
  <post_fx><tonemap curve="aces" exposure="1.0"/></post_fx>
</lighting_profile>
)xml";
    auto result = parse_lighting_profile_xml(kNoPreset);
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), LightingProfileError::MissingPreset);
}

TEST(LightingProfileParser, FailEmptyString) {
    auto result = parse_lighting_profile_xml("");
    ASSERT_TRUE(result.is_err());
    // empty string → pugixml parse failure → XmlParseError
    EXPECT_EQ(result.error(), LightingProfileError::XmlParseError);
}

// ---------------------------------------------------------------------------
// load_lighting_profile_file — file-IO error path
// (FileNotFound; FileOpenFailed cannot be reliably triggered portably in tests)
// ---------------------------------------------------------------------------

TEST(LightingProfileFileLoader, FailFileNotFound) {
    auto result = load_lighting_profile_file(
        "/t/OdysseyEngine/demo/showcase/lighting_profiles/__nonexistent__.xml");
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), LightingProfileError::FileNotFound);
}

// ---------------------------------------------------------------------------
// load_lighting_profile_file — success path using a real authored profile.
// Requires the build's copy_demo_assets step to have run.
// ---------------------------------------------------------------------------

TEST(LightingProfileFileLoader, SuccessLoadWarmth) {
    // Path relative to the build working directory (set by CMake copy_demo_assets).
    std::filesystem::path profile_path =
        "demo/showcase/lighting_profiles/warmth.xml";
    if (!std::filesystem::exists(profile_path)) {
        GTEST_SKIP() << "demo assets not copied to build dir; run copy_demo_assets";
    }
    auto result = load_lighting_profile_file(profile_path);
    ASSERT_TRUE(result.is_ok()) << "Expected warmth.xml to load cleanly";
    const auto& d = result.value();
    EXPECT_EQ(d.preset, "Warmth");
    // warmth.xml: exposure=1.00, vignette strength=0.20, grain intensity=0.03
    EXPECT_NEAR(d.tonemap_exposure,  1.00f, 1e-4f);
    EXPECT_NEAR(d.vignette_strength, 0.20f, 1e-4f);
    EXPECT_NEAR(d.grain_intensity,   0.03f, 1e-4f);
    EXPECT_FALSE(d.has_chromatic_aberration);
}

TEST(LightingProfileFileLoader, SuccessLoadDread) {
    std::filesystem::path profile_path =
        "demo/showcase/lighting_profiles/dread.xml";
    if (!std::filesystem::exists(profile_path)) {
        GTEST_SKIP() << "demo assets not copied to build dir; run copy_demo_assets";
    }
    auto result = load_lighting_profile_file(profile_path);
    ASSERT_TRUE(result.is_ok()) << "Expected dread.xml to load cleanly";
    const auto& d = result.value();
    EXPECT_EQ(d.preset, "Dread");
    // dread.xml: exposure=0.75, vignette strength=0.35, grain intensity=0.07
    // chromatic_aberration strength=0.0020
    EXPECT_NEAR(d.tonemap_exposure,     0.75f,   1e-4f);
    EXPECT_NEAR(d.vignette_strength,    0.35f,   1e-4f);
    EXPECT_NEAR(d.grain_intensity,      0.07f,   1e-4f);
    EXPECT_TRUE(d.has_chromatic_aberration);
    EXPECT_NEAR(d.chromatic_aberration, 0.0020f, 1e-5f);
}

// ---------------------------------------------------------------------------
// profile_to_crt_params — pure mapping tests
// ---------------------------------------------------------------------------

TEST(ProfileToCRTParams, ExposureScalesBaseBrightness) {
    // Derivation check: brightness_out = base_brightness * exposure
    // base=1.2, exposure=0.75 → expected=0.9
    CRTParams base{};
    base.brightness = 1.2f;
    base.vignette_strength = 0.8f;
    base.flicker_amount = 0.2f;
    base.chromatic_aberration = 1.0f;

    LightingProfileData profile{};
    profile.tonemap_exposure        = 0.75f;
    profile.vignette_strength       = 0.8f;  // same as base → unchanged
    profile.grain_intensity         = 0.2f;  // same as base → unchanged
    profile.has_chromatic_aberration = false; // not present → don't override

    CRTParams out = profile_to_crt_params(base, profile);

    EXPECT_NEAR(out.brightness, 1.2f * 0.75f, 1e-5f);
    EXPECT_NEAR(out.vignette_strength,    0.8f, 1e-5f);
    EXPECT_NEAR(out.flicker_amount,       0.2f, 1e-5f);
    EXPECT_NEAR(out.chromatic_aberration, 1.0f, 1e-5f); // unchanged
}

TEST(ProfileToCRTParams, VignetteStrengthOverridesBase) {
    CRTParams base{};
    base.vignette_strength = 0.8f;

    LightingProfileData profile{};
    profile.tonemap_exposure  = 1.0f;
    profile.vignette_strength = 0.06f;  // Liminal — very flat
    profile.grain_intensity   = 0.05f;
    profile.has_chromatic_aberration = false;

    CRTParams out = profile_to_crt_params(base, profile);
    EXPECT_NEAR(out.vignette_strength, 0.06f, 1e-5f);
}

TEST(ProfileToCRTParams, GrainMapsToFlickerAmount) {
    CRTParams base{};
    base.flicker_amount = 0.2f;

    LightingProfileData profile{};
    profile.tonemap_exposure  = 1.0f;
    profile.vignette_strength = 0.8f;
    profile.grain_intensity   = 0.07f;  // Dread grain
    profile.has_chromatic_aberration = false;

    CRTParams out = profile_to_crt_params(base, profile);
    EXPECT_NEAR(out.flicker_amount, 0.07f, 1e-5f);
}

TEST(ProfileToCRTParams, ChromaticAberrationOverridesOnlyWhenPresent) {
    CRTParams base{};
    base.chromatic_aberration = 1.5f;

    // Case 1: profile has chromatic_aberration — override.
    LightingProfileData with_chrom{};
    with_chrom.tonemap_exposure           = 1.0f;
    with_chrom.vignette_strength          = 0.8f;
    with_chrom.grain_intensity            = 0.03f;
    with_chrom.has_chromatic_aberration   = true;
    with_chrom.chromatic_aberration       = 0.0020f;

    CRTParams out_with = profile_to_crt_params(base, with_chrom);
    EXPECT_NEAR(out_with.chromatic_aberration, 0.0020f, 1e-6f);

    // Case 2: profile lacks chromatic_aberration — base passes through.
    LightingProfileData without_chrom{};
    without_chrom.tonemap_exposure         = 1.0f;
    without_chrom.vignette_strength        = 0.8f;
    without_chrom.grain_intensity          = 0.03f;
    without_chrom.has_chromatic_aberration = false;

    CRTParams out_without = profile_to_crt_params(base, without_chrom);
    EXPECT_NEAR(out_without.chromatic_aberration, 1.5f, 1e-5f);
}

TEST(ProfileToCRTParams, ExposureOneIsIdentityOnBrightness) {
    CRTParams base{};
    base.brightness = 2.3f; // arbitrary non-default

    LightingProfileData profile{};
    profile.tonemap_exposure  = 1.0f;
    profile.vignette_strength = base.vignette_strength;
    profile.grain_intensity   = base.flicker_amount;
    profile.has_chromatic_aberration = false;

    CRTParams out = profile_to_crt_params(base, profile);
    EXPECT_NEAR(out.brightness, 2.3f, 1e-5f);  // identity: 2.3 * 1.0 = 2.3
}

TEST(ProfileToCRTParams, UnchangedFieldsPassThrough) {
    // Fields that have no profile mapping (scanline_count, curvature, time)
    // must pass through from base_crt unchanged.
    CRTParams base{};
    base.time              = 42.0f;
    base.curvature         = 3.7f;
    base.scanline_weight   = 0.5f;
    base.scanline_count    = 720.0f;

    LightingProfileData profile{};
    profile.tonemap_exposure  = 1.0f;
    profile.vignette_strength = 0.8f;
    profile.grain_intensity   = 0.03f;
    profile.has_chromatic_aberration = false;

    CRTParams out = profile_to_crt_params(base, profile);
    EXPECT_NEAR(out.time,           42.0f,  1e-4f);
    EXPECT_NEAR(out.curvature,       3.7f,  1e-4f);
    EXPECT_NEAR(out.scanline_weight, 0.5f,  1e-4f);
    EXPECT_NEAR(out.scanline_count, 720.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// profile_to_eva_params — passthrough (no current mapping)
// ---------------------------------------------------------------------------

TEST(ProfileToEVAParams, PassthroughUnchanged) {
    EvaHUDParams base{};
    base.time        = 5.0f;
    base.alert_level = 0.7f;
    base.sync_ratio  = 0.9f;
    base.health_pct  = 0.5f;
    base.opacity     = 0.4f;

    LightingProfileData profile{};
    profile.preset = "Wonder";

    EvaHUDParams out = profile_to_eva_params(base, profile);
    EXPECT_NEAR(out.time,        5.0f, 1e-5f);
    EXPECT_NEAR(out.alert_level, 0.7f, 1e-5f);
    EXPECT_NEAR(out.sync_ratio,  0.9f, 1e-5f);
    EXPECT_NEAR(out.health_pct,  0.5f, 1e-5f);
    EXPECT_NEAR(out.opacity,     0.4f, 1e-5f);
}

// ---------------------------------------------------------------------------
// resolve_lighting_profile_path — pure path-resolution tests
//
// This function returns candidate paths in priority order without touching
// the filesystem. Tests are fully pure — no disk reads required.
// ---------------------------------------------------------------------------

TEST(ResolveLightingProfilePath, SceneLocalCandidateIsFirst) {
    // The scene-local path must be the first element so the caller prefers
    // an authored per-game override over the shared showcase fallback.
    namespace fs = std::filesystem;
    const fs::path scene_dir = "/some/game/scenes";
    const auto candidates = resolve_lighting_profile_path("liminal", scene_dir);
    ASSERT_EQ(candidates.size(), 2u);
    // First candidate: <scene_dir>/lighting_profiles/liminal.xml
    EXPECT_EQ(candidates[0],
              fs::path("/some/game/scenes/lighting_profiles/liminal.xml"));
}

TEST(ResolveLightingProfilePath, ShowcaseFallbackIsSecond) {
    // The showcase fallback must be the second element — used when the
    // scene's own directory has no override.
    namespace fs = std::filesystem;
    const fs::path scene_dir = "/some/game/scenes";
    const auto candidates = resolve_lighting_profile_path("wonder", scene_dir);
    ASSERT_EQ(candidates.size(), 2u);
    // Second candidate: demo/showcase/lighting_profiles/wonder.xml
    EXPECT_EQ(candidates[1],
              fs::path("demo/showcase/lighting_profiles/wonder.xml"));
}

TEST(ResolveLightingProfilePath, FilenameSuffixIsAlwaysDotXml) {
    // The function must always append ".xml" — the scene attribute stores
    // only the stem (e.g. "dread"), not the full filename.
    namespace fs = std::filesystem;
    const auto candidates = resolve_lighting_profile_path("dread", "/scenes");
    for (const auto& p : candidates) {
        EXPECT_EQ(p.extension().string(), ".xml")
            << "Expected .xml extension, got: " << p.string();
    }
}

TEST(ResolveLightingProfilePath, NameIsEmbeddedInBothCandidates) {
    // Both candidates must contain the requested profile name in their
    // filename stem so the caller can log which profile was requested.
    namespace fs = std::filesystem;
    const auto candidates = resolve_lighting_profile_path("sacred", "/scenes");
    ASSERT_EQ(candidates.size(), 2u);
    for (const auto& p : candidates) {
        EXPECT_EQ(p.stem().string(), "sacred")
            << "Expected stem 'sacred', got: " << p.string();
    }
}

TEST(ResolveLightingProfilePath, EmptySceneDirStillReturnsShowcaseFallback) {
    // When scene_dir is empty (scene was specified without a parent path,
    // e.g. just "main.scene.xml"), the scene-local candidate degrades to a
    // relative path like "lighting_profiles/name.xml".  The showcase fallback
    // must always be present and well-formed.
    namespace fs = std::filesystem;
    const auto candidates = resolve_lighting_profile_path("hostil", fs::path{});
    ASSERT_EQ(candidates.size(), 2u);
    // Fallback is always absolute relative to CWD — check it is stable.
    EXPECT_EQ(candidates[1],
              fs::path("demo/showcase/lighting_profiles/hostil.xml"));
}
