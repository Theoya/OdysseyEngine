# copy-entity

Copy the selected entity to the clipboard.

## Signature
```
/copy-entity
```

## Description
Clones the selected entity into the clipboard via Ctrl+C or Edit → Copy. The original entity remains in the scene.

## Behavior
- Calls `clipboard_copy_entity()` to clone the selected entity.
- Sets `is_cut = false` flag.
- Does not remove the entity from the scene.

## Implementation Location
- Hotkey: Ctrl+C in `src/editor/editor.cpp::draw_menu_bar()` (Edit menu)
- Clipboard: `src/editor/entity_clipboard.{h,cpp}`
