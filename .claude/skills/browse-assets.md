# /browse-assets

Navigate the two-pane asset browser with folder tree, file grid, breadcrumb, search, and type filters.

## Layout

- **Left pane**: Folder tree with `ImGui::TreeNodeEx` (OpenOnArrow flag). Click folders to select.
- **Right pane**: File grid with icon + name + type. `ImGui::Selectable` rows (~36px height).
- **Breadcrumb bar**: `<root> / sub1 / sub2` above both panes. Clickable segments navigate.
- **Search box**: Case-insensitive substring filter on filenames.
- **Type filter chips**: All · Scenes · Prefabs · Materials · Meshes · Behaviors · Lighting · Music · Other. Radio-like row.

## Selection

Clicking a folder in the left pane sets `state_.selected_folder`. Clicking a file in the right pane sets `state_.selected_asset` and wakes the Inspector.

## Double-click

- **Scene**: requests scene swap (Edit mode only)
- **Prefab**: no-op (deferred to Phase 5+)
- Other types: no-op

## Context Menu (Right-click)

- Create (submenu: Scene/Prefab/Material/Mesh/Behavior/LightingProfile/Music)
- Import
- Show in Explorer
- Delete
- Rename

## Notes

- Thumbnails are placeholder colored squares (32×32) per AssetType.
- Actual thumbnail bake is deferred (see `/thumbnail-bake` skill).
- Breadcrumb allows fast navigation without scrolling the tree.
