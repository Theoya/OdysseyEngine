# Showcase contribution — game-asset-engineer

This document pins the asset-pipeline contribution to the OdysseyEngine showcase.
It enumerates what each authored file exercises, how XSDs cover the authoring
surface, and the exact acceptance runs that gate a pass.

## 1. Files authored

All paths absolute under the repo root.

### Materials — `T:\OdysseyEngine\demo\showcase\materials\`

| File | Purpose | Exercises |
|---|---|---|
| `stone_wall.mat.xml`   | Full-PBR placeholder: albedo + metallic + roughness + normal maps. | Canonical `<shaders>`, `<pbr>` (all three scalars), `<textures>` (4 maps), BC7/BC5/BC4 format enums, sRGB + linear colorspace enums, mips attr. |
| `gold_leaf.mat.xml`    | High-metal, low-roughness contrast piece. Scalar only. | `<shaders>`, `<pbr>` with no `<textures>` branch — proves the optional-texture path round-trips without ghost nodes. |
| `painted_wood.mat.xml` | Matte, saturated, hand-painted wood. Anti-realism touchstone. | Same as stone_wall but with only two texture maps, proving per-map optionality inside `<textures>`. |

### Meshes — `T:\OdysseyEngine\demo\showcase\meshes\`

| File | Purpose | Exercises |
|---|---|---|
| `crate.mesh.xml`       | Primitive box with three LOD tiers, box auto-fit collider. | `<source format="primitive">`, `<dimensions>` (box axes), canonical `<lod><level/></lod>`, `<collider type="box" auto_fit="true">`. |
| `pillar.mesh.xml`      | Cylinder with capsule collider, 3-tier LOD. | `<source>`, `<dimensions radius height>`, capsule collider dimensions, LOD distance ramp. |
| `arena_floor.mesh.xml` | Ground plane with plane collider. | Plane-typed primitive path, plane collider, single-tier LOD. |

### Master scene — `T:\OdysseyEngine\demo\showcase\showcase.scene.xml`

One entity per archetype family plus spawn regions for the batch-spawned enemy
squads. Intentionally includes:

- Rich comment blocks (serializer preservation target).
- Unknown attributes on `<scene>` (`lighting_profile`, `audio_bank`) and on
  `<entity light_type=…>` / `<entity music_state_machine=…>` — these force the
  editor's pugixml-backed serializer down its "preserve unknown attributes"
  path, which is a major silent-regression risk.
- `count="N"` + `<spawn_region>` wrapping for the three enemy packs.
- One `<pack>` leader reference on the hunter squads.

## 2. XSD coverage matrix

Each XSD branch must be touched by at least one showcase file. `/validate-asset`
dispatches by extension; every file below must return `VALID`.

### `schemas\material.xsd`

| Element / attribute | File(s) | Branch |
|---|---|---|
| `<material name version>`           | all three            | root, required attrs |
| `<shaders><vertex src><fragment src>` | all three          | canonical shader branch |
| `<pbr><albedo color>`                 | all three          | AlbedoColorType |
| `<pbr><metallic value>`               | all three          | ScalarValueType |
| `<pbr><roughness value>`              | all three          | ScalarValueType |
| `<textures><albedo format="BC7" colorspace="srgb">`   | stone_wall, painted_wood | BC7 + srgb enums |
| `<textures><normal format="BC5" colorspace="linear">` | stone_wall, painted_wood | BC5 + linear enums |
| `<textures><metallic format="BC4">`  | stone_wall         | BC4 single-channel |
| `<textures><roughness format="BC4">` | stone_wall         | BC4 single-channel |
| no `<textures>` block                | gold_leaf          | proves optional branch |
| legacy `<shader vertex fragment>`    | existing demo/materials/*.mat.xml | legacy shader path |
| legacy `<properties><albedo>text</albedo>` | existing demo/materials/*.mat.xml | legacy properties path |

### `schemas\mesh.xsd`

| Element / attribute | File(s) | Branch |
|---|---|---|
| `<mesh name version>`                | all three          | root |
| `<source format="primitive" path>`   | all three          | SourceType canonical |
| `<dimensions width height depth>`    | crate              | box dims |
| `<dimensions radius height>`         | pillar             | cylinder dims |
| `<dimensions width depth>`           | arena_floor        | plane dims |
| canonical `<lod><level/></lod>`      | all three          | LodType |
| `<collider type="box" auto_fit>`     | crate              | box collider |
| `<collider type="capsule" radius height>` | pillar         | capsule collider |
| `<collider type="plane">`            | arena_floor        | plane collider |
| legacy `<lod_levels><lod/></lod_levels>` | existing demo/materials/*.mesh.xml | legacy LOD path |

### `schemas\skeleton.xsd`

Validated against `demo/fps_humanoid/assets/humanoid.skeleton.xml`:

| Branch | Covered |
|---|---|
| `<skeleton name>` root                         | yes |
| `<bone name parent="" ...>` root bone          | yes |
| `<bone name parent=… position rotation length radius>` | yes (19 bones) |
| `<attachment name bone position rotation>`    | yes (3 attachments: muzzle + 2 grips) |
| `xs:key` uniqueness of bone names              | yes (enforced at parse time) |

### `schemas\animation.xsd`

Validated against `demo/fps_humanoid/assets/{idle,walk_cycle}.anim.xml`:

| Branch | Covered |
|---|---|
| `<animation name duration looping>`  | idle (looping=true), walk_cycle (looping=true) |
| Multiple `<track bone>`              | walk_cycle (7 tracks), idle (2 tracks) |
| `<key time position rotation>`       | both |
| Monotonic time keyframes             | loader invariant (not XSD); pattern enforces time >= 0 |
| Default `<key scale>` = "1 1 1"      | used by all existing keys |

### `schemas\actions.xsd`

Validated against `demo/actions/{enemy_pack_hunter,multi_arm_gunner}.actions.xml`:

| Action element | Covered by |
|---|---|
| `<move_to position>`   | enemy_pack_hunter patrol, multi_arm_gunner patrol_sweep |
| `<wait duration>`      | both |
| `<play_anim id duration>` | both |
| `<play_sound name>`    | both |
| `<look_at target>`     | both |
| `<emit_signal value>`  | both |
| `<set_state key value>` | both |
| `<spawn prefab offset>` | enemy_pack_hunter death_sequence, multi_arm_gunner death_sequence |
| `<destroy_self/>`      | both death_sequence entries |
| `loop="true"` on sequence | both patrol sequences |

## 3. Round-trip acceptance

**Target:** `/roundtrip-test demo/showcase/showcase.scene.xml`

**Pass criteria:**

1. Load the scene via `odyssey::scene::load_scene_file`.
2. Serialize back via `odyssey::scene::serialize_scene` (pugixml doc-in-place).
3. `std::filesystem::file_size` matches byte-for-byte.
4. `std::memcmp` of original vs serialized buffer returns 0.

**What MUST survive:**

- Every comment block (the 5 banner `─────` comments and every inline `<!-- -->`).
- Attribute ordering as written (`id` before `archetype`, `light_type` before `kelvin`, etc.).
- Intentionally-unknown attributes: `lighting_profile`, `audio_bank`,
  `material_override`, `light_type`, `kelvin`, `flicker_amp`, `music_state_machine`,
  `initial_state`, `bus`, `volume`.
- Trailing newline on the file.

**If NOT byte-identical:** the serializer has drift. The usual culprits are
(a) attribute reordering, (b) comment stripping, (c) float re-formatting
(`1.0` -> `1`), (d) default-value injection on elements that had the attribute
absent. Fix the serializer — never rewrite the XML to match the serializer.

## 4. Hot-reload acceptance

**Target:** touch `demo/showcase/materials/stone_wall.mat.xml`, confirm the
material rebinds without an engine restart.

**Pass criteria:**

1. Engine running with showcase scene loaded (`odyssey run demo/showcase/showcase.scene.xml`).
2. Edit `stone_wall.mat.xml`: change `<metallic value="0.02"/>` to `<metallic value="0.60"/>`.
3. Save. File-watcher fires within 200 ms.
4. `MaterialRegistry::reload(path)` re-parses, re-validates, and updates the
   existing `MaterialHandle` in place — the handle integer is unchanged.
5. Next frame, the stone-wall surfaces visibly brighten (metallic=0.60 is a
   large jump).
6. `/descriptor-dump` shows the same descriptor slot; no new slot allocated,
   no orphan slot marked dirty.

**Descriptor-leak probe:** after 20 consecutive edits the descriptor occupancy
reported by `/descriptor-dump` is unchanged. A leak would grow occupancy by 1
per edit.

## 5. Texture format policy (memorialized)

All runtime textures are compressed. RGBA8 is NEVER shipped.

| Signal                      | Format | bpp  | Rationale |
|---|---|---|---|
| Albedo (authored diffuse)   | BC7    | 8    | Best-in-class perceptual quality for sRGB RGBA; adaptive endpoints + multi-partition modes; AMD/NVIDIA/Intel desktop all support natively. Ref: Microsoft DX BC docs; Fabien Giesen "BCn compression overview". |
| Tangent-space normal        | BC5    | 8    | Stores XY only; Z reconstructed as `sqrt(1 - x*x - y*y)` in shader. No chroma sub-sampling artifacts (unlike BC3nm). |
| Single-channel mask (metallic, roughness, AO) | BC4 | 4 | Single-channel block compression; 2x smaller than packing into BC5; no cross-channel leakage. |
| HDR color (environment, emissive) | BC6H | 8 | Signed/unsigned 16-bit-per-channel FP block format. Use unsigned for irradiance/emissive, signed for precomputed radiance with negative lobes. |
| Legacy RGB cutout           | BC1    | 4    | Last resort. Not used in showcase. |

**Memory math (worst case per material):** stone_wall uses 4 maps at 2048x2048
mipped:
- BC7 albedo: 2048*2048 * 1 byte = 4.19 MB, +1.40 MB mip chain = **5.59 MB**
- BC5 normal: same = **5.59 MB**
- BC4 metallic: 2048*2048 * 0.5 = 2.10 MB, + 0.70 mip = **2.80 MB**
- BC4 roughness: same = **2.80 MB**
- **Total per stone_wall instance: ~16.8 MB VRAM**

The uncompressed RGBA8 equivalent for the same 4 maps, mipped, is **~67 MB** —
a **4x reduction** before mip and channel-count tuning. Source: Khronos KTX2
specification, appendix on BCn block sizes.

## 6. How to verify (exact commands)

From repo root, with a built Debug or Release tree:

```bash
# 1. XSD validity of every showcase asset.
/validate-asset demo/showcase/materials/stone_wall.mat.xml
/validate-asset demo/showcase/materials/gold_leaf.mat.xml
/validate-asset demo/showcase/materials/painted_wood.mat.xml
/validate-asset demo/showcase/meshes/crate.mesh.xml
/validate-asset demo/showcase/meshes/pillar.mesh.xml
/validate-asset demo/showcase/meshes/arena_floor.mesh.xml
/validate-asset demo/showcase/showcase.scene.xml

# 2. Backward compatibility — existing assets still validate under the new XSDs.
/validate-asset demo/materials/enemy_boss.mat.xml
/validate-asset demo/materials/crate.mesh.xml
/validate-asset demo/fps_humanoid/assets/humanoid.skeleton.xml
/validate-asset demo/fps_humanoid/assets/walk_cycle.anim.xml
/validate-asset demo/actions/enemy_pack_hunter.actions.xml

# 3. Round-trip and hot-reload.
/roundtrip-test demo/showcase/showcase.scene.xml
# (start engine, edit stone_wall.mat.xml, observe)
/descriptor-dump   # expect stable occupancy after repeated edits
```

## 7. Failure modes (so regressions are catchable)

| Failure | What it looks like | Likely cause |
|---|---|---|
| XSD rejects a legacy mesh | `/validate-asset` reports unexpected `<lod_levels>` | mesh.xsd lost the legacy xs:choice branch |
| XSD rejects a legacy material | `/validate-asset` reports unexpected `<shader>` | material.xsd lost the legacy branch |
| Round-trip diff: attributes reordered | byte-diff shows same content, different order | serializer is regenerating attrs instead of editing in place — violates the "edit original pugixml doc" rule |
| Round-trip diff: comments gone | byte-diff shows comment blocks missing in output | serializer is rebuilding from AST without re-emitting comments |
| Round-trip diff: "1.0" vs "1" | numeric format drift | serializer is re-formatting floats instead of preserving source text |
| Hot-reload doesn't update visibly | surfaces unchanged after save | file-watcher not firing, or reload is allocating a new handle instead of mutating |
| Descriptor leak | `/descriptor-dump` occupancy grows per edit | reload path is allocating a new descriptor slot instead of reusing |
