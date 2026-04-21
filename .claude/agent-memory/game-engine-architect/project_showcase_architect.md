---
name: Showcase architect contract
description: Load-bearing decisions for the demo/showcase rendering/physics/editor contribution, including prefab schema limits and phase-4 deferrals.
type: project
---

Showcase demo at `demo/showcase/` is the canonical all-features scene. Rendering/physics/editor falls to game-engine-architect. Key pinned decisions:

**Why (motivation):** showcase is the regression gate — every engine feature has to fail here first before it regresses anywhere else. Acceptance runs (per-agent skill) feed `demo/showcase/acceptance_report.md`. Below 100% pass → council convenes before release.

**How to apply:**

- Physics path is first-principles (semi-implicit Euler + sequential impulse), explicitly **NOT Jolt**. Justification: vibe physics overrides, pure-function grain, determinism for netcode replay, future GPU compute solver.
- Bindless descriptor set uses `VK_EXT_descriptor_indexing` with `VARIABLE_DESCRIPTOR_COUNT + UPDATE_AFTER_BIND + PARTIALLY_BOUND`. Slots grow monotonically with a free-list for reuse — no mid-play compaction.
- Offscreen → ImGui sync contract: explicit semaphores on every edge, explicit `COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL` layout transition. `/barrier-audit` must show no implicit edges.
- Mode invariants: `Edit` freezes dt=0 AND skips Nadir dispatch + script tick + physics step, but keeps rendering. `Simulate` = `Play` + editor chrome visible. `Play` hides chrome.
- Hot-reload gate: GPU drain on last fence → rebuild EntityComponents → bump `scene_generation` counter → stale handles safely no-op.

**Prefab schema limits (phase-4 deferred):**

- `prefab.xsd` uses `xs:all` with `maxOccurs=1` per child. Two consequences:
  1. No `<rigidbody>` element yet — crate and player prefabs carry the intended block as a top-of-file authoring comment. `/schema-add` + `/rigidbody-add` un-stub it.
  2. Only one `<script>` per prefab. Multi-script entities (showcase player needs both PlayerController and RespawnScript) wire the second script at runtime via ScriptRunner::attach_script. Fix path: promote `<script>` to unbounded `xs:sequence`.
