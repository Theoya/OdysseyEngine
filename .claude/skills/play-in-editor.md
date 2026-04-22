# play-in-editor

Trigger Play mode and capture the scene state at the moment Play begins.

## Signature
```
/play-in-editor
```

## Description
Transitions the editor from Edit mode to Play mode, capturing a deep snapshot of the scene and all entities. The snapshot is restored when the user clicks Stop. This enables "play-test" workflows where designers can iterate without accidentally losing edits.

## Behavior
- **Before Play**: Calls `capture_snapshot(scene_data, entity_manager)` to clone the entire scene state.
- **Click Play**: Button press sets `Mode::Play`, stores the snapshot in `Editor::Impl::play_snapshot`, feeds zero delta-time to the engine if paused.
- **Click Stop**: Calls `restore_snapshot()` to roll back to the captured state, returns to `Mode::Edit`.
- **Pause/Step**: `play_paused` flag controls engine tick (dt=0 when paused); Step advances one frame.

## Implementation Location
- Buttons: `src/editor/editor.cpp::draw_mode_toolbar()`
- Snapshot: `src/editor/play_snapshot.{h,cpp}`
- State: `EditorState::play_paused`, `EditorState::play_snapshot_requested`
