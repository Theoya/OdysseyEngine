# undo

Undo the last scene mutation.

## Signature
```
/undo
```

## Description
Reverts the editor to the previous saved scene state via Ctrl+Z or Edit → Undo menu. Each undo entry captures the full scene and entity state at the moment a mutation was made.

## Behavior
- Calls `UndoStack::pop_undo()` to retrieve the previous entry.
- Restores the scene via `restore_snapshot()`.
- Moves the popped entry to the redo stack.
- Maximum 64 entries; oldest entries are trimmed automatically.

## Implementation Location
- Hotkey: Ctrl+Z in `src/editor/editor.cpp::draw_menu_bar()` (Edit menu)
- Undo stack: `src/editor/undo_stack.{h,cpp}`
