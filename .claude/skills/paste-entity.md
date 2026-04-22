# paste-entity

Paste entities from the clipboard into the scene.

## Signature
```
/paste-entity
```

## Description
Instantiates new entities in the scene from the clipboard via Ctrl+V or Edit → Paste. If the clipboard was populated by a cut (not copy), the original entities are deleted and the clipboard is cleared.

## Behavior
- Iterates over `EntityClipboard::entities`.
- For each clipped entity, calls `EntityManager::create_entity()` with cloned components.
- If `is_cut == true`, clears the clipboard after paste (move semantics).
- If `is_cut == false`, leaves clipboard intact (copy semantics; may paste again).

## Implementation Location
- Hotkey: Ctrl+V in `src/editor/editor.cpp::draw_menu_bar()` (Edit menu)
- Clipboard: `src/editor/entity_clipboard.{h,cpp}`
