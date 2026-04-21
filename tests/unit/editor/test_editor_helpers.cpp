// ---------------------------------------------------------------------------
// test_editor_helpers.cpp
//
// Unit tests for the pure helpers in src/editor/. Every function tested
// here must be a Result<T,E> / plain-value pure function — no Vulkan, no
// ImGui, no filesystem.
// ---------------------------------------------------------------------------

#include "editor/editor.h"
#include "editor/mode_enum.h"
#include "editor/log_panel.h"

#include "scene/entity_manager.h"

#include <gtest/gtest.h>

using namespace odyssey;
using namespace odyssey::editor;

// ---------------------------------------------------------------------------
// mode_enum
// ---------------------------------------------------------------------------

TEST(EditorMode, LabelsAreStableAndDistinct) {
    EXPECT_EQ(mode_label(Mode::Edit),     "Edit");
    EXPECT_EQ(mode_label(Mode::Play),     "Play");
    EXPECT_EQ(mode_label(Mode::Simulate), "Simulate");
}

TEST(EditorMode, NadirPauseSemantics) {
    EXPECT_FALSE(mode_runs_nadir(Mode::Edit));
    EXPECT_TRUE (mode_runs_nadir(Mode::Play));
    EXPECT_TRUE (mode_runs_nadir(Mode::Simulate));
}

TEST(EditorMode, ScriptPauseSemantics) {
    EXPECT_FALSE(mode_runs_scripts(Mode::Edit));
    EXPECT_TRUE (mode_runs_scripts(Mode::Play));
    // Simulate mode explicitly pauses scripts — this is the whole point
    // of the third mode: watch AI & physics without scripted influence.
    EXPECT_FALSE(mode_runs_scripts(Mode::Simulate));
}

TEST(EditorMode, PhysicsPauseSemantics) {
    EXPECT_FALSE(mode_runs_physics(Mode::Edit));
    EXPECT_TRUE (mode_runs_physics(Mode::Play));
    EXPECT_TRUE (mode_runs_physics(Mode::Simulate));
}

// ---------------------------------------------------------------------------
// entity_display_label
// ---------------------------------------------------------------------------

TEST(EntityDisplayLabel, NamedEntityGetsNameAndID) {
    scene::Entity e;
    e.id   = 42;
    e.name = "gold_pillar_center";
    EXPECT_EQ(entity_display_label(e), "gold_pillar_center  [42]");
}

TEST(EntityDisplayLabel, UnnamedEntityGetsFallbackName) {
    scene::Entity e;
    e.id   = 7;
    e.name.clear();
    EXPECT_EQ(entity_display_label(e), "entity_7  [7]");
}

TEST(EntityDisplayLabel, ZeroIDRoundTrips) {
    scene::Entity e;
    e.id   = 0;
    e.name = "player_1";
    EXPECT_EQ(entity_display_label(e), "player_1  [0]");
}

// ---------------------------------------------------------------------------
// is_static_archetype
// ---------------------------------------------------------------------------

TEST(IsStaticArchetype, RecognizedStaticNames) {
    EXPECT_TRUE(is_static_archetype("static"));
    EXPECT_TRUE(is_static_archetype("prop"));
    EXPECT_TRUE(is_static_archetype("geometry"));
}

TEST(IsStaticArchetype, DynamicNamesAreNotStatic) {
    EXPECT_FALSE(is_static_archetype("player"));
    EXPECT_FALSE(is_static_archetype("scout"));
    EXPECT_FALSE(is_static_archetype("brute"));
    EXPECT_FALSE(is_static_archetype(""));
    EXPECT_FALSE(is_static_archetype("STATIC"));  // case-sensitive by design
}

// ---------------------------------------------------------------------------
// LogRingBuffer — pure data structure
// ---------------------------------------------------------------------------

TEST(LogRingBuffer, StartsEmpty) {
    LogRingBuffer rb;
    EXPECT_EQ(rb.size(), 0u);
}

TEST(LogRingBuffer, PushesBelowCapacity) {
    LogRingBuffer rb;
    LogEntry e;
    e.level = 2;
    e.text  = "hello";
    rb.push(e);
    rb.push(e);
    rb.push(e);
    EXPECT_EQ(rb.size(), 3u);
    EXPECT_EQ(rb.at(0).text, "hello");
    EXPECT_EQ(rb.at(2).text, "hello");
}

TEST(LogRingBuffer, OverwritesOldestAtCapacity) {
    LogRingBuffer rb;
    // Fill to capacity with distinct levels so we can track what survives.
    for (size_t i = 0; i < LogRingBuffer::CAPACITY; ++i) {
        LogEntry e;
        e.level = static_cast<int>(i % 6);
        e.text  = "#" + std::to_string(i);
        rb.push(e);
    }
    EXPECT_EQ(rb.size(), LogRingBuffer::CAPACITY);
    EXPECT_EQ(rb.at(0).text, "#0");

    // Push one more — oldest should be gone, size should stay at capacity.
    LogEntry e;
    e.text = "#wrap";
    rb.push(e);
    EXPECT_EQ(rb.size(), LogRingBuffer::CAPACITY);
    EXPECT_EQ(rb.at(0).text, "#1");                          // #0 evicted
    EXPECT_EQ(rb.at(LogRingBuffer::CAPACITY - 1).text, "#wrap");
}

TEST(LogRingBuffer, ClearResetsState) {
    LogRingBuffer rb;
    LogEntry e; e.text = "x";
    rb.push(e); rb.push(e);
    ASSERT_EQ(rb.size(), 2u);
    rb.clear();
    EXPECT_EQ(rb.size(), 0u);
}
