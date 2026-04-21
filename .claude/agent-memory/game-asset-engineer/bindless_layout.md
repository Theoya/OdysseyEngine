---
name: Bindless descriptor set layout
description: Decided bindless descriptor-set layout for OdysseyEngine — VK_EXT_descriptor_indexing, set 1 = 4096 textures, SSBO for lights/materials.
type: project
---

Descriptor-set layout locked 2026-04-20 (asset_checklist §4), conditional on `VK_EXT_descriptor_indexing` being available (core 1.2, confirmed on RTX 3080):

- **Set 0** — per-frame UBO (camera, time, globals).
- **Set 1** — bindless textures. `COMBINED_IMAGE_SAMPLER × 4096` with `VARIABLE_DESCRIPTOR_COUNT_BIT | PARTIALLY_BOUND_BIT | UPDATE_AFTER_BIND_BIT`. Shader indexes via `nonuniformEXT(tex_id)`.
- **Set 2** — SSBOs: `LightBuffer`, `MaterialBuffer` (holds tex_id indices into set 1), per-archetype Nadir buffers.
- **Set 3** — reserved for push-descriptor transient per-draw data.

**Why:** LightBuffer is SSBO not UBO — UBO 64 KB cap hits ~170 lights on a 64-byte struct. SSBO has no cap and matches GPU-maximalist ethos.

**Fallback:** if `descriptorIndexing` is unavailable at device init, fall back to capped UBO-based materials (128 lights, 256 materials) with a runtime warning. No silent gameplay change.

**How to apply:** any new asset type that lives in GPU-visible storage picks SSBO by default (set 2). Only reach for UBO when the data is genuinely per-frame scalar globals (set 0). Texture registration: append to set 1 at next free slot; slot is stable for asset lifetime.
