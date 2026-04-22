# Stella Girl — source record

**Intended use:** reference / study only. Examining what Meshy AI outputs at this fidelity level so we can calibrate our own Meshy experiments (if/when we integrate Meshy into the OdysseyEngine asset pipeline per `3D_modeling_agent_tips.md`).

## Metadata

| Field | Value |
|---|---|
| Sketchfab URL | https://sketchfab.com/3d-models/stella-girl-08f81fedb4224ceab1a0424edde278ba |
| Author | [@golden4445](https://sketchfab.com/golden4445) |
| Generation tool | **Meshy AI** (high confidence) |
| License | CC-BY 4.0 |
| Retrieved | 2026-04-21 |
| Published | 2026-02-14 |
| Sketchfab stats at retrieval | 59k views, 3.8k downloads, 525 likes |
| Triangles | 447.3k |
| Vertices | 223.6k |
| Rigged | No (Meshy default, unrigged) |
| Textures | Diffuse-baked (AI-generated, not PBR-authored) |

## How the tool was identified

Cross-referenced with other work on the author's profile — specifically a sibling model literally titled "red dress girl_meshy ai" plus multiple "Ninja woman / Meshy AI" entries. Mesh signature (dense uniform topology, no rig, diffuse-only texture, 447k-tri range) matches Meshy's raw high-poly export. See the investigation agent's report for full evidence chain.

## Why this is in `reference_meshes/` not `base_humanoid/`

This is **not** a base mesh for the "array of body shapes" direction (see `memory/feedback_humanoid_base_mesh.md`). It's a finished character with clothing, hair, and pose baked into the geometry, unrigged, at ~25x our character poly budget. Re-use would require full retopo + unclothing + rigging — more work than generating a fresh MPFB variant.

Kept purely as a fidelity reference for evaluating Meshy's quality ceiling and topology behavior.

## Download instructions (for the user)

1. Open the Sketchfab URL above.
2. Sign in to your Sketchfab account (required — CC-BY downloads are gated behind login).
3. Click the **Download** button.
4. Prefer **glTF** or **Original Source** format. (Original source often ships FBX + textures; glTF is self-contained.)
5. Drop the downloaded archive here: `T:\OdysseyEngine\third_party\reference_meshes\stella_girl_meshy\`
6. Extract in place. Typical layout:
   ```
   stella_girl_meshy/
     LICENSE.txt         (here)
     SOURCE.md           (here)
     stella_girl.glb     (or .fbx + .mtl + textures)
     textures/           (if separate)
   ```

## Post-download checklist

- [ ] Binary files dropped in this directory (NOT git-added)
- [ ] Add this directory to `.gitignore` if not already covered by `third_party/` pattern
- [ ] Optional: bake a reference render in Fantasy Etherealism lighting for side-by-side comparison with MPFB output
- [ ] Optional: measure actual tri count on the downloaded mesh vs. 447k claim on Sketchfab
