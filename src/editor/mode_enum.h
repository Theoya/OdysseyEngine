#pragma once

// ---------------------------------------------------------------------------
// mode_enum.h (Phase 2 — re-export shim)
// The authoritative Mode enum now lives in src/core/mode.h so both src/app/
// (Engine) and src/editor/ can depend on it without circular dependency.
// This header exists so existing `#include "editor/mode_enum.h"` sites keep
// compiling unchanged.
// ---------------------------------------------------------------------------

#include "core/mode.h"

namespace odyssey::editor {

// Source-compatible alias for Phase 1 code.
using Mode = ::odyssey::Mode;

// Re-export pure predicates so editor code does not need to reach into
// the root namespace.
using ::odyssey::mode_label;
using ::odyssey::mode_runs_nadir;
using ::odyssey::mode_runs_scripts;
using ::odyssey::mode_runs_physics;
using ::odyssey::mode_runs_game_logic;
using ::odyssey::mode_runs_camera;

} // namespace odyssey::editor
