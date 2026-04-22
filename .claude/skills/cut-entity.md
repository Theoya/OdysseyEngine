# cut-entity

Cut the selected entity to the clipboard (copy + delete).

## Signature
```
/cut-entity
```

## Description
Copies the selected entity to the clipboard and deletes it from the scene via Ctrl+X or Edit → Cut.

## Behavior
- Calls `clipboard_copy_entity()` to clone the selected entity into `EntityClipboard`.
- Sets `is_cut = true` flag so paste will not duplicate after cut.
- Destroys the entity from `EntityManager`.
- Clears the selection.

## Implementation Location
- Hotkey: Ctrl+X in `src/editor/editor.cpp::draw_menu_bar()` (Edit menu)
- Clipboard: `src/editor/entity_clipboard.{h,cpp}`
