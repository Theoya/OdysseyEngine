# /import-obj

Import a Wavefront OBJ model and auto-generate its `.mesh.xml` descriptor.

## Usage
`/import-obj <obj_path> [--name <basename>] [--project <root>] [--collider <shape>]`

## Arguments
- `<obj_path>` — Path to the source .obj file (can be anywhere)
- `--name <basename>` — Base name for the mesh (defaults to OBJ filename stem)
- `--project <root>` — Project root directory (defaults to current editor project)
- `--collider <shape>` — Collider type: `box` (default), `sphere`, `capsule`, `mesh`, or `none`

## Workflow

### 1. Validate the source
Check that the .obj file exists and is readable.

### 2. Determine destination
The mesh is placed at `<project_root>/meshes/<basename>.obj`

### 3. Copy the OBJ file
Copy the source .obj to the destination, creating the `meshes/` directory if needed.

### 4. Generate the `.mesh.xml` descriptor
Create `<project_root>/meshes/<basename>.mesh.xml` with:
- Source format: `obj`
- Path to OBJ (relative to project root): `meshes/<basename>.obj`
- Default LOD level at distance 0
- Collider (default `box` with `auto_fit="true"`)

### 5. Refresh the Asset Browser
The newly imported mesh appears in the Meshes group.

## Generated Descriptor Template

```xml
<?xml version="1.0" encoding="UTF-8"?>
<mesh name="<basename>" version="1">
  <source format="obj" path="meshes/<basename>.obj"/>
  <lod>
    <level distance="0" triangles="0"/>
  </lod>
  <collider type="box" auto_fit="true"/>
</mesh>
```

## Examples

```bash
# Import a model with auto-detection
/import-obj "C:/exports/crate.obj"

# Custom base name
/import-obj "C:/exports/model.obj" --name my_custom_name

# Spherical collider
/import-obj "C:/exports/sphere.obj" --collider sphere

# To a specific project
/import-obj "C:/exports/weapon.obj" --project "D:/game/demo/showcase"
```

## See Also
- `/import-asset` — generic asset import (supports all types)
- `/create-mesh` — scaffold a mesh descriptor for primitive shapes
