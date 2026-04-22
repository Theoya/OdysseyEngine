# /import-asset

Import an external asset file into the project folder via the Asset Browser context menu.

## Dialog Flow

1. Show "Import Asset" file dialog (Open, not Save)
2. Filter by supported types: `*.obj` (meshes), `*.png` (textures), `*.wav`/`*.mp3` (audio), `*.xml` (generics)
3. User selects file
4. Ask for destination folder in the project (browser shows current folder)
5. Copy file to destination
6. Refresh browser and select the imported asset
7. Inspector preview opens

## Copy Behavior

- Destination path = `<project_root> / <selected_folder> / <source_filename>`
- If file exists, prompt "Overwrite?"
- Preserve original filename extension

## Notes

- Mesh imports (`.obj`) may trigger automatic material generation (deferred to Phase 5+)
- Audio imports may require format validation/conversion (WASAPI expects specific codecs)
- No move/symlink — always a physical copy for portability
