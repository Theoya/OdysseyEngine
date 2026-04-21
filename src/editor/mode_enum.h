#pragma once

// ---------------------------------------------------------------------------
// mode_enum.h
// Editor execution-mode enum. Shared between the editor and any future
// engine hooks that need to honor Play / Edit / Simulate semantics.
//
// Mode semantics (authoritative for Phase 1+):
//   Edit      — scene is loaded, GPU renders, but Nadir is paused and
//               scripts do not tick. Camera is free; you can inspect.
//   Play      — full simulation. Nadir dispatches, scripts tick, physics
//               integrates, input routes to the game.
//   Simulate  — Nadir + physics run (to observe AI/physics behavior under
//               its own steam), but scripts are paused. Useful for
//               authoring: you can watch AI without game logic pushing
//               it around.
// ---------------------------------------------------------------------------

#include <string_view>

namespace odyssey::editor {

enum class Mode {
    Edit = 0,
    Play = 1,
    Simulate = 2,
};

// Pure — no I/O, no allocation. Stable labels for UI and logging.
constexpr std::string_view mode_label(Mode m) {
    switch (m) {
        case Mode::Edit:     return "Edit";
        case Mode::Play:     return "Play";
        case Mode::Simulate: return "Simulate";
    }
    return "Unknown";
}

// Pure — returns whether Nadir compute dispatch should run in this mode.
constexpr bool mode_runs_nadir(Mode m) {
    return m == Mode::Play || m == Mode::Simulate;
}

// Pure — returns whether scripts should tick in this mode.
constexpr bool mode_runs_scripts(Mode m) {
    return m == Mode::Play;
}

// Pure — returns whether physics integration should run.
constexpr bool mode_runs_physics(Mode m) {
    return m == Mode::Play || m == Mode::Simulate;
}

} // namespace odyssey::editor
