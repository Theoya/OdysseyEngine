#pragma once

// ---------------------------------------------------------------------------
// core/mode.h
// Execution mode — shared across editor, engine, scripting, and physics.
//
// Lives in src/core/ (pure types) so that src/app/ (Engine) and src/editor/
// can both depend on it WITHOUT creating a circular dependency. src/editor/
// re-exports `odyssey::editor::Mode` as an alias for source-compatibility
// with Phase 1.
//
// Mode semantics (authoritative):
//   Edit      — scene is loaded, rendering happens, but Nadir does NOT
//               dispatch, scripts do NOT tick, and physics does NOT step.
//               Camera input remains live so the user can fly around.
//   Play      — full simulation. Nadir dispatches, scripts tick, physics
//               integrates.
//   Simulate  — Nadir + physics run, scripts paused. For tuning AI without
//               scripts side-effecting the scene.
//
// All predicates below are pure (no I/O, constexpr).
// ---------------------------------------------------------------------------

#include <string_view>

namespace odyssey {

enum class Mode {
    Edit     = 0,
    Play     = 1,
    Simulate = 2,
};

// Pure — stable label for UI / logging.
constexpr std::string_view mode_label(Mode m) {
    switch (m) {
        case Mode::Edit:     return "Edit";
        case Mode::Play:     return "Play";
        case Mode::Simulate: return "Simulate";
    }
    return "Unknown";
}

// Pure — whether Nadir compute dispatch should run in this mode.
constexpr bool mode_runs_nadir(Mode m) {
    return m == Mode::Play || m == Mode::Simulate;
}

// Pure — whether scripts should tick in this mode.
constexpr bool mode_runs_scripts(Mode m) {
    return m == Mode::Play;
}

// Pure — whether physics integration should step this mode.
constexpr bool mode_runs_physics(Mode m) {
    return m == Mode::Play || m == Mode::Simulate;
}

// Pure — whether per-entity scripts on the GameManager side should tick.
// (Used by game harness tests — identical to mode_runs_scripts but named
// for test-readability when asserting game-level behavior.)
constexpr bool mode_runs_game_logic(Mode m) {
    return m == Mode::Play;
}

// Pure — whether the free-fly camera input should be consumed. In Phase 2
// we keep camera live in every mode so the user can always navigate.
constexpr bool mode_runs_camera(Mode /*m*/) {
    return true;
}

} // namespace odyssey
