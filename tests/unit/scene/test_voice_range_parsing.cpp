// ---------------------------------------------------------------------------
// test_voice_range_parsing.cpp
//
// Loader-side contract for the `voice_range` attribute on <stats>, ratified
// by docs/decisions/2026-04-20-proximity-voice-chat.md (condition
// "asset — schema round-trip").
//
// Authored-time contract, enforced by scene_loader::parse_scene_xml:
//   * Absent attribute            → Ok(default = 25.0m = d_max)
//   * Valid in-range value        → Ok(value)
//   * Malformed ("abc", trailing) → Err
//   * Negative                    → Err
//   * > 50.0                      → Err
//   * NaN                         → Err
//   * Infinity                    → Err
//
// The XSD encodes the same [0, 50] bound but is documentary — pipelines
// without XSD validation (MCP tools, CLI scaffolds, procedural scenes) rely
// on the loader to reject bad authoring. Runtime server clamp at 50m is a
// separate concern and lives under src/net/ (netcode owns that file set).
// ---------------------------------------------------------------------------

#include "scene/scene_loader.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace odyssey;
using namespace odyssey::scene;

namespace {

// Build a minimal scene XML with a single <stats> carrying the given
// voice_range attribute spelling. Passing nullptr omits the attribute.
std::string scene_with_voice_range(const char* vr_literal) {
    std::ostringstream s;
    s << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<scene name=\"t\" version=\"1\">\n"
      << "  <entity id=\"e1\" archetype=\"player\">\n"
      << "    <stats health=\"100\" max_health=\"100\"";
    if (vr_literal != nullptr) {
        s << " voice_range=\"" << vr_literal << "\"";
    }
    s << "/>\n"
      << "  </entity>\n"
      << "</scene>\n";
    return s.str();
}

} // namespace

// ---------------------------------------------------------------------------
// Happy paths.
// ---------------------------------------------------------------------------

TEST(VoiceRangeParsing, ExplicitIntegerValueParses) {
    auto r = parse_scene_xml(scene_with_voice_range("10"));
    ASSERT_TRUE(r.is_ok()) << r.error();
    const auto& scene = r.value();
    ASSERT_EQ(scene.entities.size(), 1u);
    EXPECT_FLOAT_EQ(scene.entities[0].voice_range, 10.0f);
}

TEST(VoiceRangeParsing, ExplicitFractionalValueParses) {
    auto r = parse_scene_xml(scene_with_voice_range("12.5"));
    ASSERT_TRUE(r.is_ok()) << r.error();
    EXPECT_FLOAT_EQ(r.value().entities[0].voice_range, 12.5f);
}

TEST(VoiceRangeParsing, AbsentAttributeYieldsDocumentedDefault25m) {
    // d_max = 25m per docs/design/proximity_chat_audio.md.
    auto r = parse_scene_xml(scene_with_voice_range(nullptr));
    ASSERT_TRUE(r.is_ok()) << r.error();
    ASSERT_EQ(r.value().entities.size(), 1u);
    EXPECT_FLOAT_EQ(r.value().entities[0].voice_range, 25.0f);
}

TEST(VoiceRangeParsing, BoundaryZeroAccepted) {
    // 0m means "this entity is never audible via proximity voice". Valid.
    auto r = parse_scene_xml(scene_with_voice_range("0"));
    ASSERT_TRUE(r.is_ok()) << r.error();
    EXPECT_FLOAT_EQ(r.value().entities[0].voice_range, 0.0f);
}

TEST(VoiceRangeParsing, BoundaryFiftyAccepted) {
    // 50m is the authored-time maximum (server also hard-clamps to 50m).
    auto r = parse_scene_xml(scene_with_voice_range("50"));
    ASSERT_TRUE(r.is_ok()) << r.error();
    EXPECT_FLOAT_EQ(r.value().entities[0].voice_range, 50.0f);
}

TEST(VoiceRangeParsing, MissingStatsNodeLeavesDefault) {
    // If <stats> itself is omitted, voice_range should still be 25.0f.
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<scene name=\"t\" version=\"1\">\n"
        "  <entity id=\"e1\" archetype=\"player\"/>\n"
        "</scene>\n";
    auto r = parse_scene_xml(xml);
    ASSERT_TRUE(r.is_ok()) << r.error();
    ASSERT_EQ(r.value().entities.size(), 1u);
    EXPECT_FLOAT_EQ(r.value().entities[0].voice_range, 25.0f);
}

// ---------------------------------------------------------------------------
// Failure paths — every one of these MUST produce Result::err, never a silent
// fallback. Rationale: malformed voice_range would cause hard-to-diagnose
// audio pathologies (inaudible, infinite-range, NaN in DSP chain).
// ---------------------------------------------------------------------------

TEST(VoiceRangeParsing, MalformedAlphaReturnsErr) {
    auto r = parse_scene_xml(scene_with_voice_range("abc"));
    ASSERT_TRUE(r.is_err());
    EXPECT_NE(r.error().find("voice_range"), std::string::npos)
        << "error should mention voice_range: " << r.error();
}

TEST(VoiceRangeParsing, TrailingGarbageReturnsErr) {
    // std::stof happily parses "12abc" as 12 and leaves the tail. Our parser
    // rejects that explicitly because trailing garbage is almost always a
    // typo ("12m", "12 meters") that we want caught at authoring time.
    auto r = parse_scene_xml(scene_with_voice_range("12abc"));
    EXPECT_TRUE(r.is_err());
}

TEST(VoiceRangeParsing, NegativeReturnsErr) {
    auto r = parse_scene_xml(scene_with_voice_range("-1"));
    ASSERT_TRUE(r.is_err());
    EXPECT_NE(r.error().find("voice_range"), std::string::npos);
}

TEST(VoiceRangeParsing, NegativeSmallFractionReturnsErr) {
    auto r = parse_scene_xml(scene_with_voice_range("-0.0001"));
    EXPECT_TRUE(r.is_err());
}

TEST(VoiceRangeParsing, OverMaxReturnsErr) {
    // 100 > 50 hard cap. XSD rejects too; loader is belt-and-braces.
    auto r = parse_scene_xml(scene_with_voice_range("100"));
    ASSERT_TRUE(r.is_err());
    EXPECT_NE(r.error().find("voice_range"), std::string::npos);
}

TEST(VoiceRangeParsing, JustOverMaxReturnsErr) {
    auto r = parse_scene_xml(scene_with_voice_range("50.0001"));
    EXPECT_TRUE(r.is_err());
}

TEST(VoiceRangeParsing, NaNReturnsErr) {
    auto r = parse_scene_xml(scene_with_voice_range("nan"));
    ASSERT_TRUE(r.is_err());
    EXPECT_NE(r.error().find("voice_range"), std::string::npos);
}

TEST(VoiceRangeParsing, InfinityReturnsErr) {
    auto r = parse_scene_xml(scene_with_voice_range("inf"));
    EXPECT_TRUE(r.is_err());
}

TEST(VoiceRangeParsing, EmptyStringFallsBackToDefault) {
    // An explicitly empty voice_range="" is indistinguishable from "author
    // wanted the default". Loader treats it as default rather than erroring.
    auto r = parse_scene_xml(scene_with_voice_range(""));
    ASSERT_TRUE(r.is_ok()) << r.error();
    EXPECT_FLOAT_EQ(r.value().entities[0].voice_range, 25.0f);
}

TEST(VoiceRangeParsing, ErrorMessageCitesOffendingEntity) {
    // When voice_range fails, the error must name the entity so authors and
    // agents can jump straight to the broken node without line-hunting.
    auto r = parse_scene_xml(scene_with_voice_range("-5"));
    ASSERT_TRUE(r.is_err());
    EXPECT_NE(r.error().find("e1"), std::string::npos)
        << "error should name the offending entity id: " << r.error();
}
