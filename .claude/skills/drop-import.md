# /drop-import

Drag-drop assets from OS Explorer onto the OdysseyEngine editor window for import.

## Usage

1. Open OS Explorer (Windows File Explorer)
2. Navigate to any folder containing asset files
3. Select one or more files (supported formats: `.obj`, `.png`, `.jpg`, `.jpeg`, `.wav`, `.glb`, `.fbx`, `.xml`)
4. Drag the file(s) onto the editor window
5. Assets are imported to the project tree and the Asset Browser is refreshed

## Supported Formats

| Extension | Destination |
|-----------|-------------|
| .obj | `meshes/<name>.obj` (+ `.mesh.xml` generated) |
| .png | `textures/<name>.png` |
| .jpg, .jpeg | `textures/<name>.[jpg\|jpeg]` |
| .wav | `audio/<name>.wav` |
| .glb | `imported/<name>.glb` |
| .fbx | `imported/<name>.fbx` |
| .xml | `imported/<name>.xml` |

## Implementation Details

### GLFW Drop Callback
- Registered in `Editor::initialize` via `glfwSetDropCallback`
- Receives file paths from OS
- Invokes `execute_import` for each dropped file (idempotent, no overwrite by default)

### Behavior
- Multiple files can be dropped at once
- Each file is processed independently
- Directories are created automatically
- Descriptors (.mesh.xml) are generated for .obj files
- Asset Browser refreshes after successful imports
- Errors are logged but do not stop other files from being imported

### Collision Handling
- Existing files are rejected unless `--overwrite` flag is set
- Error message is logged but does not interrupt the drag-drop operation

## Examples

1. **Single OBJ Model**
   - Drag `my_model.obj` from Explorer onto the editor
   - File copied to `meshes/my_model.obj`
   - Descriptor auto-generated at `meshes/my_model.mesh.xml`

2. **Texture Batch**
   - Select multiple PNG files in Explorer
   - Drag onto editor
   - All copied to `textures/` folder

3. **Audio File**
   - Drag `background.wav` onto editor
   - Copied to `audio/background.wav`

## Notes

- Drag-drop is immediate; no confirmation dialog shown
- Paths are displayed in the editor's log
- The project root is determined by the editor's current session
- Symlinks and shortcuts are not supported; only physical file copies

## See Also
- `/import-asset` — open dialog-based import instead of drag-drop
- `/import-obj` — specifically for OBJ → mesh descriptor workflow
