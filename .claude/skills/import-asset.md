# /import-asset

Import an external asset file into the OdysseyEngine editor project.

## Entry Points

1. **File → Import Asset…** menu in the menu bar
2. **Asset Browser → Import…** button in the panel toolbar
3. Drag-drop files from OS Explorer onto the editor window

## Supported Formats

| Extension | Destination | Action |
|-----------|-------------|--------|
| .obj | `meshes/<basename>.obj` | Copy file + auto-generate `.mesh.xml` descriptor |
| .png | `textures/<basename>.png` | Copy only |
| .jpg, .jpeg | `textures/<basename>.[jpg\|jpeg]` | Copy only |
| .wav | `audio/<basename>.wav` | Copy only |
| .glb | `imported/<basename>.glb` | Copy only (loader TBD) |
| .fbx | `imported/<basename>.fbx` | Copy only (loader TBD) |
| .xml | `imported/<basename>.xml` | Copy only (validation TBD) |

## Implementation Details

### Pure Logic Layer
- `plan_import(source, project_root)` → determines destination paths
- `classify_import_source(source)` → identifies asset type
- `generate_mesh_xml_for_obj(obj_path)` → produces canonical `.mesh.xml` XML text

### Impure I/O Layer
- `execute_import(source, project_root, overwrite=false)` → copies file, writes descriptor, creates directories
- File dialogs via Win32 `GetOpenFileNameA` (all files filter available)
- GLFW drop callback registered in `Editor::initialize`

### Workflow
1. User opens dialog or drags file
2. `execute_import` validates source exists
3. `plan_import` resolves destination(s) based on extension
4. File is copied to target; directories created as needed
5. If .obj, `.mesh.xml` descriptor is written (relative path to obj from project root)
6. Asset Browser is refreshed to show new asset

## Error Handling

- Unsupported extension → `Result::err("unsupported extension: .<ext>")`
- File not found → `Result::err("source file not found: ...")`
- Target exists without overwrite → `Result::err("target file already exists: ...")`
- Directory creation failure → `Result::err("failed to create directory: ...")`
- Copy failure → `Result::err("failed to copy file: ...")`

## Notes

- No destination folder selection dialog; destination is determined by extension (automatic routing)
- `.mesh.xml` names the OBJ by its stem; collider defaults to `auto_fit="true" type="box"`
- Triple-extension artifacts (e.g., ".mesh.xml") handled correctly in basename extraction
- All paths normalized to forward slashes in descriptors for cross-platform correctness
