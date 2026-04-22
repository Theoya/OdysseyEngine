# 3D Asset Modeler — Memory Index

- [Skill library](skills/INDEX.md) — persistent skill procedures (+3 cloth-sim skills added 2026-04-22)
- [MPFB2 extension install path (RESOLVED 2026-04-21)](project_mpfb2_blender_version_blocker.md) — MPFB2 works in Blender 4.2+ as an EXTENSION (not legacy add-on). Install via `bpy.ops.extensions.package_install_files`. Module id is `bl_ext.user_default.mpfb`.
- [Purge glTF_not_exported scaffolding before saving](feedback_clean_gltf_scaffolding.md) — glTF importer leaves helper geometry (icospheres, pivots) in a collection that's invisible at data-block level but visible at view-layer level; clean it up or the reopened blend looks empty/wrong.
