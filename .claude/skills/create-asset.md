# /create-asset

Create a new asset from the Asset Browser right-click context menu.

## Asset Types

| Type | File Extension | Scaffold |
|------|---|---|
| Scene | `.scene.xml` | Empty scene with `<scene/>` root |
| Prefab | `.prefab.xml` | Empty prefab with `<prefab/>` root |
| Material | `.mat.xml` | Default material (PBR albedo, metallic, roughness) |
| Mesh | `.mesh.xml` | Primitive shape selector (box, sphere, plane, cylinder) |
| Behavior | `.nadir` | Empty GLSL compute shader with preamble |
| Lighting Profile | `lighting_profiles/*.xml` | Default lighting (ambient + directional light) |
| Music | `.music.xml` | Music state machine (idle/explore/combat states) |

## Dialog Flow

1. Show a modal "Create New Asset"
2. Ask for filename (text input) and asset type (radio buttons)
3. Generate the scaffold in the selected folder
4. Refresh the browser and select the new asset
5. Inspector preview opens to the new file

## Notes

- Filenames must not exist (error if duplicate)
- Filenames are auto-validated (no path separators, no special characters)
- Scaffolds follow the schema (validate with `/validate-asset` if needed)
