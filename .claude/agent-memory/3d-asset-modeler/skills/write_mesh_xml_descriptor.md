---
skill: write_mesh_xml_descriptor
difficulty: beginner
prerequisites: [an exported .obj file; familiarity with schemas/mesh.xsd]
status: complete — authored 2026-04-21
blender target: N/A (author-time)
---

## Goal

Author an engine-valid `.mesh.xml` descriptor for an OdysseyEngine-consumable asset. The descriptor is a thin XML pointer at a primitive keyword or an external file (OBJ or glTF). The engine's `src/assets/mesh_loader.cpp` (tiny_obj backend) consumes these descriptors.

## Sources

1. Internal: `schemas/mesh.xsd` — authoritative schema. Must be read first.
2. Internal: `demo/showcase/assets/berserk_halo_mk3/berserk_halo.mesh.xml` — reference OBJ-sourced descriptor, known-good in the current engine.
3. Internal: `demo/materials/*.mesh.xml` — 50+ existing primitive descriptors, canonical author patterns for box/sphere/cylinder.
4. Internal: `src/assets/mesh_loader.cpp` — the actual loader. Reading this is the most honest source; comments document the legacy short form.
5. Internal: `CLAUDE.md` "mesh format is descriptor-only XML" section.

External "source count" for this skill is trivially met via internal engine code — this is a pure-internal skill.

## Consensus ordered steps

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- one-line provenance comment — tool, date, generator -->
<mesh name="base_male" version="1">
  <source format="obj" path="third_party/base_humanoid/male/base_male.obj"/>
  <lod>
    <level distance="0"   triangles="26756"/>
    <level distance="15"  triangles="8000"/>
    <level distance="45"  triangles="2000"/>
  </lod>
  <collider type="capsule" radius="0.35" height="1.91"/>
</mesh>
```

Fields:
- `name` (required, unique identifier — matches the mesh name downstream).
- `version` (required, positive int — engine assumes `1` unless proven otherwise).
- `<source format="primitive|obj|gltf" path="..."/>` — primitive allows `"box|sphere|cylinder|capsule|ground"` as `path`; `obj`/`gltf` take a path relative to the engine working-dir (T:\OdysseyEngine).
- `<lod>` / `<level>` — distance thresholds (metres from camera) and triangle budget at that LOD. Used only by the (future) skinned-mesh and OBJ draw paths; the primitive forward renderer ignores LOD.
- `<collider>` — optional physics hint: `type="box|sphere|capsule|plane|mesh"` + type-appropriate dims. For humanoids use `capsule` with radius ~0.30-0.40 m, height = character Z-extent.

## Gotchas

- **Trailing newline matters**. The loader hasn't been verified tolerant of missing trailing newline; always end the file with `\n`.
- **Path separator**: use forward slashes `/` even on Windows. The loader does not path-normalize.
- **Paths are relative to the engine working-directory `T:\OdysseyEngine`**, not to the .mesh.xml file itself.
- **`schemas/mesh.xsd` has an XML-comment-double-hyphen bug on line ~13** — `lxml.etree.XMLSchema(open(...).read())` will reject the schema file itself. For validation, parse-and-shape-match via pugixml, or strip the comment before passing to lxml.
- The current forward renderer (`src/render/*`) only binds primitive `mesh_type` enums `{box=0, sphere=1, ground=2, cylinder=3}`. An OBJ-source descriptor is **parsed** but not drawn. Writing an OBJ-source .mesh.xml is a staging act for the future skinned-mesh renderer, not a live draw trigger.
- **Triangle count in `<level triangles="N">`** should be the count AFTER triangulation. An OBJ with 13378 quad-faces triangulates to ~26756 tris on engine load. When in doubt, use `2 * quad_face_count` as the LOD0 estimate.
- Legacy short form `<source format="primitive">box</source>` (text content, not attribute) is still accepted by the loader; the schema has a `LodLevelsLegacyType` for the same reason. New descriptors should use the canonical form (attribute).

## Post-task update log

- 2026-04-21 — Session 9613f92d — Authored `demo/fps_humanoid/assets/base_male.mesh.xml` and `base_female.mesh.xml` pointing at the MPFB-generated OBJs. Both shape-match the schema; neither tested via the XSD because the schema file still has the double-hyphen comment bug.
