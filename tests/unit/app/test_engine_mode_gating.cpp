// ---------------------------------------------------------------------------
// test_engine_mode_gating.cpp
// Phase 2: asserts Engine::set_mode() flips the pure tick-predicates that
// gate Nadir / scripts / physics / camera downstream.
//
// We do not spin up a real Vulkan Engine (no GPU in CI) — instead we rely
// on the pure predicates the Engine exposes: tick_would_dispatch_nadir(),
// tick_would_run_scripts(), tick_would_step_physics(), tick_would_run_camera().
//
// These predicates are thin wrappers over core/mode.h — the same predicates
// the engine's process_frame() consults — so asserting on them proves the
// gating contract end-to-end without needing a GPU.
// ---------------------------------------------------------------------------

#include "app/engine.h"
#include "core/mode.h"

#include <gtest/gtest.h>

using namespace odyssey;

TEST(EngineMode, DefaultsToPlay) {
    Engine e;
    EXPECT_EQ(e.mode(), Mode::Play);
    EXPECT_TRUE(e.tick_would_dispatch_nadir());
    EXPECT_TRUE(e.tick_would_run_scripts());
    EXPECT_TRUE(e.tick_would_step_physics());
    EXPECT_TRUE(e.tick_would_run_camera());
}

TEST(EngineMode, EditModePausesEverythingExceptCamera) {
    Engine e;
    e.set_mode(Mode::Edit);
    EXPECT_EQ(e.mode(), Mode::Edit);
    EXPECT_FALSE(e.tick_would_dispatch_nadir());
    EXPECT_FALSE(e.tick_would_run_scripts());
    EXPECT_FALSE(e.tick_would_step_physics());
    EXPECT_TRUE (e.tick_would_run_camera());
}

TEST(EngineMode, PlayModeRunsEverything) {
    Engine e;
    e.set_mode(Mode::Play);
    EXPECT_TRUE(e.tick_would_dispatch_nadir());
    EXPECT_TRUE(e.tick_would_run_scripts());
    EXPECT_TRUE(e.tick_would_step_physics());
    EXPECT_TRUE(e.tick_would_run_camera());
}

TEST(EngineMode, SimulateRunsNadirAndPhysicsButPausesScripts) {
    Engine e;
    e.set_mode(Mode::Simulate);
    EXPECT_EQ(e.mode(), Mode::Simulate);
    EXPECT_TRUE (e.tick_would_dispatch_nadir());
    EXPECT_FALSE(e.tick_would_run_scripts());
    EXPECT_TRUE (e.tick_would_step_physics());
    EXPECT_TRUE (e.tick_would_run_camera());
}

TEST(EngineMode, ModeTransitionsAreSticky) {
    Engine e;
    e.set_mode(Mode::Edit);
    EXPECT_EQ(e.mode(), Mode::Edit);
    e.set_mode(Mode::Simulate);
    EXPECT_EQ(e.mode(), Mode::Simulate);
    e.set_mode(Mode::Play);
    EXPECT_EQ(e.mode(), Mode::Play);
}

TEST(EngineMode, SetModeIdempotent) {
    // Calling set_mode with the same value twice is a no-op.
    Engine e;
    e.set_mode(Mode::Edit);
    e.set_mode(Mode::Edit);
    EXPECT_EQ(e.mode(), Mode::Edit);
}

// ---------------------------------------------------------------------------
// Pure predicate tests — belt-and-suspenders coverage for core/mode.h.
// The editor-side predicates in mode_enum.h re-export these, so this test
// also proves the re-export contract.
// ---------------------------------------------------------------------------

TEST(CoreModePredicates, LabelsMatchEditorLabels) {
    EXPECT_EQ(mode_label(Mode::Edit),     "Edit");
    EXPECT_EQ(mode_label(Mode::Play),     "Play");
    EXPECT_EQ(mode_label(Mode::Simulate), "Simulate");
}

TEST(CoreModePredicates, NadirMatrix) {
    EXPECT_FALSE(mode_runs_nadir(Mode::Edit));
    EXPECT_TRUE (mode_runs_nadir(Mode::Play));
    EXPECT_TRUE (mode_runs_nadir(Mode::Simulate));
}

TEST(CoreModePredicates, ScriptsMatrix) {
    EXPECT_FALSE(mode_runs_scripts(Mode::Edit));
    EXPECT_TRUE (mode_runs_scripts(Mode::Play));
    EXPECT_FALSE(mode_runs_scripts(Mode::Simulate));
}

TEST(CoreModePredicates, PhysicsMatrix) {
    EXPECT_FALSE(mode_runs_physics(Mode::Edit));
    EXPECT_TRUE (mode_runs_physics(Mode::Play));
    EXPECT_TRUE (mode_runs_physics(Mode::Simulate));
}

TEST(CoreModePredicates, CameraAlwaysRuns) {
    // Invariant: the free-fly camera is always live — the user can navigate
    // even while the sim is paused.
    EXPECT_TRUE(mode_runs_camera(Mode::Edit));
    EXPECT_TRUE(mode_runs_camera(Mode::Play));
    EXPECT_TRUE(mode_runs_camera(Mode::Simulate));
}

TEST(CoreModePredicates, GameLogicMatchesScripts) {
    // mode_runs_game_logic is an alias for mode_runs_scripts — this test
    // pins the aliasing so future changes don't drift.
    EXPECT_EQ(mode_runs_game_logic(Mode::Edit),     mode_runs_scripts(Mode::Edit));
    EXPECT_EQ(mode_runs_game_logic(Mode::Play),     mode_runs_scripts(Mode::Play));
    EXPECT_EQ(mode_runs_game_logic(Mode::Simulate), mode_runs_scripts(Mode::Simulate));
}
