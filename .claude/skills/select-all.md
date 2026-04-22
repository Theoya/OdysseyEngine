# select-all

Select all entities in the scene.

## Signature
```
/select-all
```

## Description
Populates `EditorState::multi_selected` with all entity IDs in the scene via Ctrl+A or Edit → Select All.

## Behavior
- Iterates over `EntityManager::get_all_entities()`.
- Inserts each entity ID into `state_.multi_selected`.
- Clears the single selection (`selected_entity = INVALID_ENTITY`).

## Implementation Location
- Hotkey: Ctrl+A in `src/editor/editor.cpp::draw_menu_bar()` (Edit menu)
- Selection: `EditorState::multi_selected`
