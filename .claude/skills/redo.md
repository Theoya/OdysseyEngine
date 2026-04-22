# redo

Redo the last undone scene mutation.

## Signature
```
/redo
```

## Description
Restores the most recently undone scene state via Ctrl+Y or Edit → Redo menu.

## Behavior
- Calls `UndoStack::pop_redo()` to retrieve the next redo entry.
- Restores the scene via `restore_snapshot()`.
- Moves the popped entry back to the undo stack.

## Implementation Location
- Hotkey: Ctrl+Y in `src/editor/editor.cpp::draw_menu_bar()` (Edit menu)
- Redo stack: `src/editor/undo_stack.{h,cpp}`
