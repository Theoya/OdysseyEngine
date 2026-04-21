# OdysseyEngine - Claude Code Project Guide

## Build
```bash
# CRITICAL: must set VCToolsVersion to match vcpkg binary ABI
export VCToolsVersion=14.42.34433

# Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=T:/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
VCToolsVersion=14.42.34433 cmake --build build --config Release

# Run
cd build && ./Release/odyssey.exe run
```

## Test
```bash
cd build && ./Release/odyssey_tests_unit.exe    # 118 unit tests
cd build && ./Release/odyssey_tests_pipeline.exe # pipeline tests (requires GPU)
```

## Architecture
- **GPU-maximalist**: behavior AI runs as compute shaders (.nadir files)
- **Pure functions**: all C++ functions are pure; side effects isolated to I/O boundaries
- **Nadir system**: GLSL compute shaders evaluate all behaviors simultaneously with weighted scoring (not behavior trees)

## Key Directories
- `src/core/` - types, Result<T,E>
- `src/vulkan/` - Vulkan abstraction (instance, device, swapchain, buffer, pipeline, command)
- `src/nadir/` - behavior system (compiler, buffers, dispatch)
- `src/scene/` - scene/prefab loading, entity manager
- `src/scripting/` - C++ script system (Script, ScriptContext, ScriptResult)
- `src/net/` - networking (UDP socket, protocol, server, client, replication)
- `src/mcp/` - MCP server for Claude Code integration
- `src/debug/` - profiler, overlay, behavior replay
- `src/cli/` - CLI interface
- `src/app/` - engine main loop
- `behaviors/lib/` - GLSL include library
- `behaviors/shaders/` - .nadir behavior files
- `demo/` - shooter demo (scenes, prefabs, materials, scripts)

## Conventions
- C++20, `#pragma once`, namespaces: `odyssey::*`
- Include paths relative to `src/`
- Pure functions return values; impure wrappers commit to GPU/OS/network
- XML for all asset formats (scene, prefab, material, mesh)
- .nadir extension for behavior shaders (valid GLSL compute with auto-prepended preamble)

## Implementation Delegation Protocol

**All advisory/design agents MUST delegate hands-on coding to the `council-implementation-coder` agent.** The 7 council agents (game-ai-engineer, game-asset-engineer, game-engine-architect, lighting-mood-architect, marty-odonnell-composer, netcode-engineer, vibe-story-guardian), the `3d-asset-modeler`, and any other advisory agent are design/domain-reasoning roles. When their recommendation requires actual file edits, code writes, test additions, or build/run work in the OdysseyEngine codebase, they MUST route that work to `council-implementation-coder` via the Agent tool — they do not write code themselves.

**Why this split:** The advisory agents run on Opus for deep domain reasoning; the coder runs on Sonnet for fast, cheap execution. Opus-level thinking is wasted on mechanical file edits, and Sonnet-level thinking is wrong for design. Keeping them separate makes iteration fast *and* thoughtful — Opus plans, Sonnet ships.

**Applies to:** C++20, GLSL/`.nadir` shaders, XML assets (scene/prefab/material/mesh/skeleton/animation/actions/music/lighting_profile), XSD schemas, CMake, tests, and any file under `src/`, `demo/`, `behaviors/`, `schemas/`, `shaders/`, `tests/`, or `docs/decisions/`.

**Does not apply to:** the advisory agent's own persistent memory files under `.claude/agent-memory/<agent>/`, or documents the agent itself is explicitly asked to author (design notes, tips docs, charter entries). An agent may also hand the coder a pre-written code snippet as part of the spec.

**Workflow:** advisory agent produces an approved spec (inline in the conversation or as a `docs/decisions/<YYYY-MM-DD>-<slug>.md` record), then spawns `council-implementation-coder` with that spec and waits for the Implementation Report. Do not re-open design debate at implementation time; if scope has grown, the coder escalates back to the caller to re-convene the council.

## Engineering Mandates
1. **Pure/lean functions by default.** Any function that can be pure, is pure. Side effects isolated to thin I/O boundary wrappers (GPU submit, file I/O, socket send, audio hardware). Pure layer exhaustively unit-tested; impure layer integration-tested.
2. **Success + failure tests.** Every `Result<T,E>`-returning function gets at least one expected-success test per success path and one expected-failure test per distinct error mode.
3. **First-principles math.** No opaque formulas. Every math block has a derivation comment. If a team member can't derive it on a whiteboard, it doesn't ship.
4. **Everything understood.** No third-party library we can't explain line-by-line. No Jolt, no miniaudio/OpenAL/FMOD, PBR is opt-in (Lambertian + Blinn-Phong is default). Each new dep triggers a council vote. pugixml/shaderc/glm/VMA/ImGui grandfathered in.

## Platform Scope
**Windows only.** No macOS, Linux, or cross-platform abstraction layers until a council vote lifts this. Audio = WASAPI only. File watching = `ReadDirectoryChangesW`. Build pin: `VCToolsVersion=14.42.34433`, MSVC 2022.

## Council Protocol
Before any non-trivial engine or game design change, invoke `/council` and wait for tally (see `C:\Users\THadfield\.claude\skills\council\SKILL.md`).

**Triggers (auto-invoke):** new subsystem directory under `src/`, new public API in existing subsystem header, schema change (`schemas/*.xsd`, engine.xml shape), new dependency in `vcpkg.json`, render pipeline change (shader semantics, render pass structure, UBO/SSBO layout), netcode protocol change (version bump, tick/send-rate, snapshot field add/remove/reorder, authority-model flip), replacement of an existing library, new enemy archetype / named theme / major visual shader / tone-establishing scene, or any change marked `// DESIGN:` in the request.

**Not triggers:** typo fixes, private impl refactors, test-only changes, doc prose.

**Rule:** 80% weighted consensus required. Below 80% → escalate to user with `escalation_template.md`. Record every decision in `docs/decisions/<YYYY-MM-DD>-<slug>.md`.

### Council weights
- game-ai-engineer: 2
- game-asset-engineer: 2
- game-engine-architect: 2 normally, 3 when topic is physics
- lighting-mood-architect: 3
- marty-odonnell-composer: 4 (can force escalation single-handed)
- netcode-engineer: 2
- vibe-story-guardian: 2
- 3d-asset-modeler: 2 (added 2026-04-21)
- council-implementation-coder: 1 (implementation feasibility lens only)
- user: 4 (tiebreaker; invoked on escalation)

**Totals (excluding user):** non-physics = 20, physics = 21. The coder's single vote reflects its narrow role — it weighs in only on whether a proposal is mechanically implementable under the mandates, not on design merit.

### Agent-bound skills (invoke when working in the agent's domain)
See agent cards at `C:\Users\THadfield\.claude\skills\council\agents\*.md` for each agent's bound skill list.

### Memory pointers
The user's auto-memory at `C:\Users\THadfield\.claude\projects\T--OdysseyEngine\memory\` contains critical project state:
- `feedback_council_rules.md` — weights, threshold, tally math
- `project_audio_direction.md` — first-principles mixer, 4-API MusicDirector (Marty-led)
- `project_engineering_mandates.md` — the four mandates in full
- `project_platform_scope.md` — Windows-only, WASAPI, ReadDirectoryChangesW
