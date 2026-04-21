---
name: New asset type ritual (11 steps)
description: Mandatory sequence when adding a new asset class to OdysseyEngine — skipping any step produces half-wired assets.
type: feedback
---

When introducing a new asset class (particle system, decal, terrain patch, etc.), these 11 steps must all fire. Half-wired assets are the failure mode.

1. XSD first — shape defined before any loader code.
2. Pure parser: `Result<T> parse_<type>_xml(string_view)` — no I/O, success+failure unit tests.
3. Impure loader: `Result<T> load_<type>_file(path)` — thin wrapper.
4. Asset registry entry: BCn format, descriptor slot policy, generation-counter bucket.
5. Serializer: `save_<type>(T, original_doc, path)` — in-place edits only.
6. Inspector sub-editor — all fields addressable, all mutations through `UndoStack`.
7. `/create-<type>` skill — agent-parallel scaffolding.
8. `/validate-<type>` dispatch row added to `/validate-asset`.
9. Wire into `scene.xsd`/`prefab.xsd` via `/schema-add` four-step ritual if entity-scoped.
10. Hot-reload + generation-counter test.
11. Council notification (every schema change is a council trigger).

**Why:** the failure mode is silent data loss — parser works, render works, but save strips the field, or the Inspector can't edit it, or the schema accepts nonsense. Each step closes one gap; skipping any opens one.

**How to apply:** this is enforced by the `/schema-add` skill for entity-scoped additions and by code review for standalone asset types. When reviewing a new-asset PR, checklist the 11 steps explicitly — do not approve until all are green.

Reference: `C:\Users\THadfield\.claude\agent-knowledge\game-asset-engineer\asset_checklist.md` §7 (canonical version; update that doc when refining the ritual).
