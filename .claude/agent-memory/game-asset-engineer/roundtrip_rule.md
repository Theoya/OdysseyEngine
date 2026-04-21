---
name: Serializer round-trip invariant
description: Every XML asset serializer must edit the original pugixml document in place — never regenerate from the in-memory struct. Defaults are never emitted.
type: feedback
---

Rule: every `save_<type>(current, pugi::xml_document& original_doc, path)` function MUST edit the original document in place. Never regenerate from the struct. Never call `remove` + `append` when `set_value` suffices.

**Why:** round-trip safety is the stronger invariant. Regeneration loses comments, attribute order, unknown attributes, and authorial whitespace. These cannot be reconstructed from the loaded struct because the struct doesn't carry them. The editor-mutable plan makes the serializer the bottleneck for every saved asset — if it's lossy, the whole editor is lossy. Also: the round-trip unit test harness uses strict byte-diff and catches regressions immediately.

**How to apply:**
- Load with pugixml, keep the `xml_document` alive until save.
- `set_value` on existing attributes; `append_attribute` only for truly new ones.
- Never emit an attribute equal to the XSD default (inflates every diff).
- Float format: max 6 sig-figs, no trailing zeros, decimal always present (`0.9`, `1.0`, not `0.900000` or `1`).
- Save with `format_raw | format_no_empty_element_tags`; copy the original's terminal byte.
- Atomic: write to `<path>.tmp`, fsync, rename.

The `/roundtrip-test` skill is the canonical verification. Any failure is a serializer bug, never a data bug.
