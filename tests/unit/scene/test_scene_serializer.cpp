// ---------------------------------------------------------------------------
// test_scene_serializer.cpp
// Phase 2: load → serialize byte-identical round-trip tests.
//
// The contract: on an unmutated SceneData, serialize_scene_to_string()
// returns the exact bytes we loaded. Proven against:
//   (a) demo/showcase/showcase.scene.xml — deep "preserve unknowns" test,
//       hits lighting stubs, audio stubs, scene-root attrs, comments.
//   (b) demo/scenes/shooter_arena.scene.xml — regression gate for the
//       existing shooter demo's scene shape.
//
// We also test the happy/error paths on serialize_scene (file I/O boundary)
// and exercise the explicit force_reconstruct path for serialize_scene_to_string.
// ---------------------------------------------------------------------------

#include "scene/scene_loader.h"
#include "scene/scene_serializer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace odyssey;
using namespace odyssey::scene;

namespace {

// Read a file in binary mode into a string. Matches the loader's read mode
// so comparisons are byte-identical on Windows (no CRLF normalization).
std::string read_file_binary(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::in | std::ios::binary);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Locate a demo file relative to the test binary's cwd. CMake copies `demo/`
// into the build tree via copy_demo_assets.
std::filesystem::path demo_path(const char* rel) {
    std::filesystem::path p1 = rel;
    if (std::filesystem::exists(p1)) return p1;
    std::filesystem::path p2 = std::filesystem::path("..") / rel;
    if (std::filesystem::exists(p2)) return p2;
    // Fall through — tests will fail informatively.
    return p1;
}

} // namespace

// ---------------------------------------------------------------------------
// Round-trip: showcase.scene.xml
// ---------------------------------------------------------------------------

TEST(SceneSerializer, ShowcaseRoundTripByteIdentical) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    ASSERT_TRUE(std::filesystem::exists(path))
        << "Fixture missing: " << path.string();

    auto loaded = load_scene_file(path);
    ASSERT_TRUE(loaded.is_ok()) << loaded.error();
    auto scene = std::move(loaded).value();
    EXPECT_FALSE(scene.mutated);
    EXPECT_FALSE(scene.preserved_source.empty());

    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok()) << ser.error();

    std::string expected = read_file_binary(path);
    EXPECT_EQ(std::move(ser).value(), expected);
}

TEST(SceneSerializer, ShowcaseKnownLightAttributesCaptured) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    ASSERT_TRUE(std::filesystem::exists(path));
    auto loaded = load_scene_file(path);
    ASSERT_TRUE(loaded.is_ok());
    auto scene = std::move(loaded).value();

    // Find the `sun` entity (light_type=directional, kelvin=5500, etc.).
    const SceneData::EntityDesc* sun = nullptr;
    for (const auto& e : scene.entities) {
        if (e.id == "sun") { sun = &e; break; }
    }
    ASSERT_NE(sun, nullptr);
    EXPECT_EQ(sun->archetype, "light");

    auto find = [&](const char* k) -> const std::string* {
        for (const auto& [kk, vv] : sun->unknown_attributes) {
            if (kk == k) return &vv;
        }
        return nullptr;
    };
    ASSERT_NE(find("light_type"), nullptr);
    EXPECT_EQ(*find("light_type"), "directional");
    ASSERT_NE(find("direction"), nullptr);
    EXPECT_EQ(*find("direction"), "-0.3 -1 -0.5");
    ASSERT_NE(find("kelvin"), nullptr);
    EXPECT_EQ(*find("kelvin"), "5500");
    ASSERT_NE(find("intensity"), nullptr);
    EXPECT_EQ(*find("intensity"), "3.0");
}

TEST(SceneSerializer, ShowcaseTorchFlickerAttributesCaptured) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    ASSERT_TRUE(std::filesystem::exists(path));
    auto loaded = load_scene_file(path);
    ASSERT_TRUE(loaded.is_ok());
    auto scene = std::move(loaded).value();

    const SceneData::EntityDesc* torch = nullptr;
    for (const auto& e : scene.entities) {
        if (e.id == "torch_north") { torch = &e; break; }
    }
    ASSERT_NE(torch, nullptr);
    auto get = [&](const char* k) -> std::string {
        for (const auto& [kk, vv] : torch->unknown_attributes)
            if (kk == k) return vv;
        return {};
    };
    EXPECT_EQ(get("light_type"), "point");
    EXPECT_EQ(get("kelvin"),     "1900");
    EXPECT_EQ(get("intensity"),  "8.0");
    EXPECT_EQ(get("range"),      "15");
    EXPECT_EQ(get("flicker_amp"), "0.15");
    EXPECT_EQ(get("flicker_hz"),  "6");
}

TEST(SceneSerializer, ShowcaseFlashlightConeAttributesCaptured) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    auto scene = load_scene_file(path).value();
    const SceneData::EntityDesc* fl = nullptr;
    for (const auto& e : scene.entities)
        if (e.id == "flashlight") { fl = &e; break; }
    ASSERT_NE(fl, nullptr);
    auto get = [&](const char* k) -> std::string {
        for (const auto& [kk, vv] : fl->unknown_attributes)
            if (kk == k) return vv;
        return {};
    };
    EXPECT_EQ(get("light_type"),  "spot");
    EXPECT_EQ(get("kelvin"),      "4200");
    EXPECT_EQ(get("intensity"),   "12.0");
    EXPECT_EQ(get("range"),       "25");
    EXPECT_EQ(get("cone_inner"),  "18");
    EXPECT_EQ(get("cone_outer"),  "32");
}

TEST(SceneSerializer, ShowcaseAudioAttributesCaptured) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    auto scene = load_scene_file(path).value();

    const SceneData::EntityDesc* md = nullptr;
    const SceneData::EntityDesc* bed = nullptr;
    for (const auto& e : scene.entities) {
        if (e.id == "music_director")  md  = &e;
        if (e.id == "ambient_bed")     bed = &e;
    }
    ASSERT_NE(md, nullptr);
    ASSERT_NE(bed, nullptr);

    auto of = [&](const SceneData::EntityDesc& d, const char* k) {
        for (const auto& [kk, vv] : d.unknown_attributes)
            if (kk == k) return vv;
        return std::string{};
    };
    EXPECT_EQ(of(*md,  "music_state_machine"),
              "demo/showcase/music/showcase.music.xml");
    EXPECT_EQ(of(*md,  "initial_state"), "explore");
    EXPECT_EQ(of(*bed, "bus"),           "ambience");
    EXPECT_EQ(of(*bed, "loop"),          "true");
    EXPECT_EQ(of(*bed, "src"),           "demo/showcase/music/stems/arena_ambience.ogg");
    EXPECT_EQ(of(*bed, "volume"),        "0.6");
}

TEST(SceneSerializer, ShowcaseGoldPillarMaterialOverrideCaptured) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    auto scene = load_scene_file(path).value();
    const SceneData::EntityDesc* gp = nullptr;
    for (const auto& e : scene.entities)
        if (e.id == "gold_pillar_center") { gp = &e; break; }
    ASSERT_NE(gp, nullptr);
    auto get = [&](const char* k) -> std::string {
        for (const auto& [kk, vv] : gp->unknown_attributes)
            if (kk == k) return vv;
        return {};
    };
    EXPECT_EQ(get("material_override"),
              "demo/showcase/materials/gold_leaf.mat.xml");
}

TEST(SceneSerializer, ShowcaseSceneRootAttributesCaptured) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    auto scene = load_scene_file(path).value();
    // Scene root attributes that are NOT {name, version} are preserved.
    auto get = [&](const char* k) -> std::string {
        for (const auto& [kk, vv] : scene.unknown_scene_attributes)
            if (kk == k) return vv;
        return {};
    };
    EXPECT_EQ(get("lighting_profile"), "liminal");
    EXPECT_EQ(get("audio_bank"),       "showcase_bank");
}

// ---------------------------------------------------------------------------
// Round-trip: shooter_arena.scene.xml (regression gate)
// ---------------------------------------------------------------------------

TEST(SceneSerializer, ShooterArenaRoundTripByteIdentical) {
    auto path = demo_path("demo/scenes/shooter_arena.scene.xml");
    ASSERT_TRUE(std::filesystem::exists(path))
        << "Fixture missing: " << path.string();

    auto loaded = load_scene_file(path);
    ASSERT_TRUE(loaded.is_ok()) << loaded.error();
    auto scene = std::move(loaded).value();

    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok());
    std::string expected = read_file_binary(path);
    EXPECT_EQ(std::move(ser).value(), expected);
}

// ---------------------------------------------------------------------------
// serialize_scene (file I/O boundary)
// ---------------------------------------------------------------------------

TEST(SceneSerializer, SerializeToTempFileMatchesSource) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    auto scene = load_scene_file(path).value();

    auto tmp = std::filesystem::temp_directory_path() /
               "odyssey_test_scene_roundtrip.scene.xml";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    auto r = serialize_scene(scene, tmp);
    ASSERT_TRUE(r.is_ok()) << r.error();
    ASSERT_TRUE(std::filesystem::exists(tmp));

    std::string expected = read_file_binary(path);
    std::string actual   = read_file_binary(tmp);
    EXPECT_EQ(actual, expected);

    std::filesystem::remove(tmp, ec);
}

TEST(SceneSerializer, SerializeToUnwritablePathReturnsError) {
    SceneData empty;
    empty.name = "empty";
    empty.mutated = true; // force reconstruction so preserved_source isn't used

    // Reserved name on Windows — cannot be created as a real file in most
    // filesystems. The error path returns a Result::err() — we don't care
    // about the exact message, only that it is_err().
    std::filesystem::path bad = "Z:/this/path/does/not/exist/scene.xml";
    auto r = serialize_scene(empty, bad);
    EXPECT_TRUE(r.is_err());
}

// ---------------------------------------------------------------------------
// force_reconstruct path — the Phase 4 authoring path.
// ---------------------------------------------------------------------------

TEST(SceneSerializer, ForceReconstructProducesValidParseableXml) {
    auto path = demo_path("demo/scenes/shooter_arena.scene.xml");
    auto scene = load_scene_file(path).value();

    SerializeOptions opts;
    opts.force_reconstruct = true;
    auto ser = serialize_scene_to_string(scene, opts);
    ASSERT_TRUE(ser.is_ok());
    std::string xml = std::move(ser).value();
    EXPECT_FALSE(xml.empty());
    EXPECT_NE(xml.find("<scene"), std::string::npos);
    EXPECT_NE(xml.find("</scene>"), std::string::npos);

    // The reconstruction must itself be parseable and carry the same entity
    // count as the original. Byte-for-byte equality is NOT expected on this
    // path — that's the whole point of the preserved_source echo contract.
    auto reparsed = parse_scene_xml(xml);
    ASSERT_TRUE(reparsed.is_ok());
    EXPECT_EQ(reparsed.value().entities.size(), scene.entities.size());
}

TEST(SceneSerializer, MutatedFlagSwitchesToReconstruction) {
    auto path = demo_path("demo/scenes/shooter_arena.scene.xml");
    auto scene = load_scene_file(path).value();
    // Force mutation path.
    scene.mutated = true;

    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok());
    // Reconstruction won't equal the original (different indentation,
    // stripped comments) — but it MUST still be valid, parseable, and
    // carry the same entity count.
    auto xml = std::move(ser).value();
    auto reparsed = parse_scene_xml(xml);
    ASSERT_TRUE(reparsed.is_ok()) << reparsed.error();
    EXPECT_EQ(reparsed.value().entities.size(), scene.entities.size());
}

TEST(SceneSerializer, InMemorySceneWithoutSourceUsesReconstruction) {
    SceneData s;
    s.name = "tiny";
    s.version = 1;
    s.gravity = vec3{0.f, -9.81f, 0.f};
    // No preserved_source, not mutated — serializer should fall through to
    // reconstruction because preserved_source is empty.
    auto ser = serialize_scene_to_string(s);
    ASSERT_TRUE(ser.is_ok());
    std::string xml = std::move(ser).value();
    EXPECT_NE(xml.find("name=\"tiny\""), std::string::npos);
    EXPECT_NE(xml.find("<world>"), std::string::npos);
    // Parse-back sanity.
    auto re = parse_scene_xml(xml);
    EXPECT_TRUE(re.is_ok());
}

TEST(SceneSerializer, InvalidXmlReturnsError) {
    std::string junk = "<scene<<not-xml";
    auto r = parse_scene_xml(junk);
    EXPECT_TRUE(r.is_err());
}

// ---------------------------------------------------------------------------
// voice_range round-trip coverage.
//
// Contract (docs/decisions/2026-04-20-proximity-voice-chat.md):
//   * A scene that authored voice_range=X round-trips byte-identical.
//   * A scene that did NOT author voice_range never grows a phantom
//     voice_range="25" on the way out (byte-identical round-trip).
//   * A mutated SceneData with voice_range != 25 must emit the attribute on
//     the reconstruction path; with voice_range == 25 must omit it.
// ---------------------------------------------------------------------------

namespace {

std::string build_voice_range_scene(const char* voice_range_literal) {
    // minimal scene, single entity, <stats> optionally carries voice_range.
    std::ostringstream s;
    s << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<scene name=\"vr\" version=\"1\">\n"
      << "  <entity id=\"p\" archetype=\"player\">\n"
      << "    <stats health=\"100\" max_health=\"100\"";
    if (voice_range_literal) {
        s << " voice_range=\"" << voice_range_literal << "\"";
    }
    s << "/>\n"
      << "  </entity>\n"
      << "</scene>\n";
    return s.str();
}

} // namespace

TEST(SceneSerializer, VoiceRangeAuthoredRoundTripsByteIdentical) {
    // The preserved_source echo path means a scene with voice_range=10
    // serializes back exactly as loaded, bytes intact.
    std::string src = build_voice_range_scene("10");
    auto loaded = parse_scene_xml(src);
    ASSERT_TRUE(loaded.is_ok()) << loaded.error();
    auto scene = std::move(loaded).value();
    // parse_scene_xml does not populate preserved_source (load_scene_file
    // does). Simulate the file-load round-trip by stamping it ourselves.
    scene.preserved_source = src;
    EXPECT_FALSE(scene.mutated);
    EXPECT_FLOAT_EQ(scene.entities.at(0).voice_range, 10.0f);

    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok()) << ser.error();
    EXPECT_EQ(std::move(ser).value(), src);
}

TEST(SceneSerializer, VoiceRangeOmittedRoundTripsWithoutPhantomAttribute) {
    // The anti-regression test: adding voice_range must NOT cause scenes
    // that never authored the attribute to acquire voice_range="25" on
    // write-back. Verified both via the echo path and the reconstruction
    // path (force_reconstruct + mutated simulation).
    std::string src = build_voice_range_scene(nullptr);
    auto loaded = parse_scene_xml(src);
    ASSERT_TRUE(loaded.is_ok());
    auto scene = std::move(loaded).value();
    scene.preserved_source = src;
    EXPECT_FLOAT_EQ(scene.entities.at(0).voice_range, 25.0f);

    // Echo path: byte-identical.
    auto echo = serialize_scene_to_string(scene);
    ASSERT_TRUE(echo.is_ok());
    EXPECT_EQ(std::move(echo).value(), src);

    // Reconstruction path: voice_range must still not appear.
    SerializeOptions opts;
    opts.force_reconstruct = true;
    auto rec = serialize_scene_to_string(scene, opts);
    ASSERT_TRUE(rec.is_ok());
    std::string out = std::move(rec).value();
    EXPECT_EQ(out.find("voice_range"), std::string::npos)
        << "phantom voice_range attribute appeared on reconstruction path:\n"
        << out;
}

TEST(SceneSerializer, MutatedVoiceRangeEmitsAttributeOnReconstruction) {
    // Start from an unauthored scene, flip voice_range on the in-memory
    // SceneData, then reserialize: the attribute MUST appear.
    std::string src = build_voice_range_scene(nullptr);
    auto loaded = parse_scene_xml(src);
    ASSERT_TRUE(loaded.is_ok());
    auto scene = std::move(loaded).value();

    scene.entities.at(0).voice_range = 12.0f;
    scene.mutated = true;

    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok());
    std::string out = std::move(ser).value();
    EXPECT_NE(out.find("voice_range=\"12\""), std::string::npos)
        << "voice_range should be emitted on the reconstruction path:\n"
        << out;

    // And the round-trip of the emitted value must parse back identically.
    auto reparsed = parse_scene_xml(out);
    ASSERT_TRUE(reparsed.is_ok()) << reparsed.error();
    EXPECT_FLOAT_EQ(reparsed.value().entities.at(0).voice_range, 12.0f);
}

TEST(SceneSerializer, MutatedVoiceRangeBackToDefaultOmitsAttribute) {
    // Symmetric case: flip voice_range from some non-default back to the
    // documented 25.0 default — the reconstruction path must omit it again.
    std::string src = build_voice_range_scene("10");
    auto loaded = parse_scene_xml(src);
    ASSERT_TRUE(loaded.is_ok());
    auto scene = std::move(loaded).value();

    scene.entities.at(0).voice_range = 25.0f;
    scene.mutated = true;

    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok());
    std::string out = std::move(ser).value();
    EXPECT_EQ(out.find("voice_range"), std::string::npos)
        << "voice_range default should be omitted, not emitted:\n" << out;
}

// ---------------------------------------------------------------------------
// Phase 4: <tag> round-trip coverage.
//
// Contract:
//   * A scene that authored N tags on one entity round-trips byte-identical.
//   * The reconstruction path emits tags in authored order with the correct
//     XML shape (`<tag name="..."/>`).
//   * Mutating tags + flipping `mutated` → reconstruction preserves the new
//     tag list (parse-back round-trip).
// ---------------------------------------------------------------------------

namespace {

std::string build_tags_scene(std::initializer_list<const char*> tag_names) {
    std::ostringstream s;
    s << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<scene name=\"tg\" version=\"1\">\n"
      << "  <entity id=\"player\" archetype=\"player\">\n";
    for (auto* t : tag_names) {
        s << "    <tag name=\"" << t << "\"/>\n";
    }
    s << "  </entity>\n"
      << "</scene>\n";
    return s.str();
}

} // namespace

TEST(SceneSerializer, TagsAuthoredRoundTripByteIdentical) {
    // 3 tags — matches the Phase 4 acceptance target in the decision record.
    std::string src = build_tags_scene({"interactive", "flammable", "quest_item"});
    auto loaded = parse_scene_xml(src);
    ASSERT_TRUE(loaded.is_ok()) << loaded.error();
    auto scene = std::move(loaded).value();
    scene.preserved_source = src;

    ASSERT_EQ(scene.entities.size(), 1u);
    const auto& tags = scene.entities.at(0).tags;
    ASSERT_EQ(tags.size(), 3u);
    EXPECT_EQ(tags[0], "interactive");
    EXPECT_EQ(tags[1], "flammable");
    EXPECT_EQ(tags[2], "quest_item");

    // Echo path — byte-identical.
    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok());
    EXPECT_EQ(std::move(ser).value(), src);
}

TEST(SceneSerializer, TagsReconstructionEmitsCorrectShape) {
    // Force reconstruction: verify the emitted XML carries the tags with the
    // expected `<tag name="..."/>` shape and in authored order.
    std::string src = build_tags_scene({"alpha", "beta"});
    auto loaded = parse_scene_xml(src);
    ASSERT_TRUE(loaded.is_ok());
    auto scene = std::move(loaded).value();

    SerializeOptions opts;
    opts.force_reconstruct = true;
    auto ser = serialize_scene_to_string(scene, opts);
    ASSERT_TRUE(ser.is_ok());
    const std::string out = std::move(ser).value();

    // Both tags appear, in order.
    size_t pa = out.find("<tag name=\"alpha\"/>");
    size_t pb = out.find("<tag name=\"beta\"/>");
    ASSERT_NE(pa, std::string::npos) << out;
    ASSERT_NE(pb, std::string::npos) << out;
    EXPECT_LT(pa, pb);

    // Round-trip parse: same tag list.
    auto re = parse_scene_xml(out);
    ASSERT_TRUE(re.is_ok()) << re.error();
    ASSERT_EQ(re.value().entities.size(), 1u);
    const auto& got = re.value().entities.at(0).tags;
    ASSERT_EQ(got.size(), 2u);
    EXPECT_EQ(got[0], "alpha");
    EXPECT_EQ(got[1], "beta");
}

TEST(SceneSerializer, TagsMutationRoundTrips) {
    // Start from a scene with two tags. Add one, remove one, flip mutated,
    // serialize, parse back, verify the new list.
    std::string src = build_tags_scene({"one", "two"});
    auto loaded = parse_scene_xml(src);
    ASSERT_TRUE(loaded.is_ok());
    auto scene = std::move(loaded).value();

    // Inspector-like mutation:
    auto& tags = scene.entities.at(0).tags;
    tags.erase(tags.begin());           // remove "one"
    tags.emplace_back("three");          // append "three"
    scene.mutated = true;

    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok());
    auto re = parse_scene_xml(std::move(ser).value());
    ASSERT_TRUE(re.is_ok());
    const auto& got = re.value().entities.at(0).tags;
    ASSERT_EQ(got.size(), 2u);
    EXPECT_EQ(got[0], "two");
    EXPECT_EQ(got[1], "three");
}

TEST(SceneSerializer, NoTagsMeansNoTagElements) {
    // A scene with zero tags must not grow phantom <tag> elements on either
    // the echo path or the reconstruction path.
    std::string src =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<scene name=\"nt\" version=\"1\">\n"
        "  <entity id=\"x\" archetype=\"player\"/>\n"
        "</scene>\n";
    auto loaded = parse_scene_xml(src);
    ASSERT_TRUE(loaded.is_ok());
    auto scene = std::move(loaded).value();
    scene.preserved_source = src;

    auto echo = serialize_scene_to_string(scene);
    ASSERT_TRUE(echo.is_ok());
    EXPECT_EQ(std::move(echo).value(), src);

    SerializeOptions opts; opts.force_reconstruct = true;
    auto rec = serialize_scene_to_string(scene, opts);
    ASSERT_TRUE(rec.is_ok());
    EXPECT_EQ(rec.value().find("<tag"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Phase 4: programmatic transform mutation round-trip.
//
// The acceptance flow: open showcase.scene.xml, mutate one entity's
// transform.position, serialize, load back, verify the change landed AND no
// other field drifted.
// ---------------------------------------------------------------------------

TEST(SceneSerializer, ShowcaseTransformMutationRoundTrip) {
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    ASSERT_TRUE(std::filesystem::exists(path));

    auto loaded = load_scene_file(path);
    ASSERT_TRUE(loaded.is_ok()) << loaded.error();
    auto scene = std::move(loaded).value();

    // Pick a known entity in the showcase — the arena floor.
    SceneData::EntityDesc* target = nullptr;
    size_t target_idx = 0;
    for (size_t i = 0; i < scene.entities.size(); ++i) {
        if (scene.entities[i].id == "arena_floor") {
            target = &scene.entities[i];
            target_idx = i;
            break;
        }
    }
    ASSERT_NE(target, nullptr);

    // Snapshot every other entity to verify non-drift afterwards.
    auto before = scene.entities;

    // Mutate just the position.
    const vec3 new_pos{12.5f, 0.25f, -3.75f};
    target->transform.position = new_pos;
    scene.mutated = true;

    // Serialize (reconstruction path) then parse back.
    auto ser = serialize_scene_to_string(scene);
    ASSERT_TRUE(ser.is_ok()) << ser.error();
    auto re = parse_scene_xml(std::move(ser).value());
    ASSERT_TRUE(re.is_ok()) << re.error();
    auto& reloaded = re.value();

    ASSERT_EQ(reloaded.entities.size(), scene.entities.size());

    // The mutated entity has the new position.
    ASSERT_TRUE(target_idx < reloaded.entities.size());
    const auto& after = reloaded.entities[target_idx];
    EXPECT_EQ(after.id, "arena_floor");
    EXPECT_FLOAT_EQ(after.transform.position.x, new_pos.x);
    EXPECT_FLOAT_EQ(after.transform.position.y, new_pos.y);
    EXPECT_FLOAT_EQ(after.transform.position.z, new_pos.z);

    // Every OTHER entity's id + archetype + position matches the original.
    for (size_t i = 0; i < reloaded.entities.size(); ++i) {
        if (i == target_idx) continue;
        EXPECT_EQ(reloaded.entities[i].id,        before[i].id);
        EXPECT_EQ(reloaded.entities[i].archetype, before[i].archetype);
        EXPECT_FLOAT_EQ(reloaded.entities[i].transform.position.x,
                        before[i].transform.position.x);
        EXPECT_FLOAT_EQ(reloaded.entities[i].transform.position.y,
                        before[i].transform.position.y);
        EXPECT_FLOAT_EQ(reloaded.entities[i].transform.position.z,
                        before[i].transform.position.z);
    }
}

TEST(SceneSerializer, FileRoundTripAfterMutation) {
    // End-to-end: load → mutate position → serialize_scene (file) → reload →
    // value is persisted. This is the exact flow Ctrl+S in the editor takes.
    auto path = demo_path("demo/showcase/showcase.scene.xml");
    auto scene = load_scene_file(path).value();
    // Mutate.
    SceneData::EntityDesc* arena = nullptr;
    for (auto& d : scene.entities) if (d.id == "arena_floor") arena = &d;
    ASSERT_NE(arena, nullptr);
    arena->transform.position = vec3{5.0f, 0.0f, 5.0f};
    scene.mutated = true;

    auto tmp = std::filesystem::temp_directory_path() /
               "odyssey_test_phase4_mutate_roundtrip.scene.xml";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    auto w = serialize_scene(scene, tmp);
    ASSERT_TRUE(w.is_ok()) << w.error();

    // Reload the written file and check the value persisted.
    auto reloaded = load_scene_file(tmp);
    ASSERT_TRUE(reloaded.is_ok()) << reloaded.error();
    const SceneData::EntityDesc* re_arena = nullptr;
    for (const auto& d : reloaded.value().entities) {
        if (d.id == "arena_floor") { re_arena = &d; break; }
    }
    ASSERT_NE(re_arena, nullptr);
    EXPECT_FLOAT_EQ(re_arena->transform.position.x, 5.0f);
    EXPECT_FLOAT_EQ(re_arena->transform.position.y, 0.0f);
    EXPECT_FLOAT_EQ(re_arena->transform.position.z, 5.0f);

    std::filesystem::remove(tmp, ec);
}
