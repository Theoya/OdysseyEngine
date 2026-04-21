---
name: BCn texture format policy
description: Runtime texture format choices per role for OdysseyEngine on RTX 3080 target — BC7/BC5/BC4/BC6H, never RGBA8 at runtime.
type: project
---

Runtime compressed-texture policy locked in 2026-04-20 (asset_checklist §3):

- **Albedo (sRGB):** BC7 UNORM sRGB, 8 bpp. 4× smaller than RGBA8, only BCn that handles colored alpha cleanly.
- **Normal maps:** BC5 UNORM two-channel, 8 bpp. Z reconstructed shader-side as `sqrt(1 − x² − y²)`. Avoids BC1/3 blue-channel smearing.
- **Metallic / roughness (single-channel):** BC4 UNORM, 4 bpp. 8× smaller than RGBA8; quantization imperceptible for near-binary data. Plan: pack MR into one BC5 once bindless set is wired.
- **HDR / emissive:** BC6H UF16, 8 bpp. Preserves >1.0 values for bloom.
- **UI / icons:** BC7 sRGB.

Authoring-time sources stay `.png`/`.tga`/`.exr`; runtime assets are `.ktx2`. RTX 3080 decodes all BCn + BC6H/BC7 natively.

**Why:** full first-principles justification in `C:\Users\THadfield\.claude\agent-knowledge\game-asset-engineer\asset_checklist.md` §3. VRAM budget: 256 2K textures @ BC7 ≈ 1 GB vs 3 GB @ RGBA8.

**How to apply:** every texture authored for runtime must pick one of these formats. Reject any `.png`/`.tga` direct-upload requests; convert to `.ktx2` first. Hard-fail in loader if RGBA8 uncompressed is loaded at runtime outside editor thumbnail path.
