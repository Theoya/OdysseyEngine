# quick-search

**Ctrl+P command palette** for the OdysseyEngine Editor (Batch H).

## Summary
Opens a searchable command palette with fuzzy-matching. Type to filter, arrow keys to navigate, Enter to execute. Auto-closes with Esc or after execution.

## Capabilities
- **Fuzzy search**: char-by-char forward scan with bonuses for sequential matches and start-of-word
- **30+ built-in commands**: File, Edit, Hierarchy, Inspector, Viewport, Layout, Window, Mode, Help
- **Live filtering**: Results sorted by match quality (best first)
- **Keybind display**: Every command shows its keyboard shortcut for reference

## Typical Workflow
1. Press **Ctrl+P** to open the palette
2. Type a few chars (e.g., "save", "play", "help")
3. Select the command and press Enter (or click)
4. The command executes and the palette closes

## Example Queries
- `save` → File: Save
- `frame` → Viewport: Frame Selected
- `play` → Mode: Play
- `undo` → Edit: Undo
- `about` → Help: About

## Implementation Details
- Pure fuzzy-scoring algorithm: `fuzzy_score(query, label)` returns 0 (no match) or positive score
- Pure filtering: `filter_commands(items, query)` returns sorted list of matches
- Command registry: `CommandRegistry` holds 30+ `CommandItem` descriptors (id, label, keybind, invoke)
- Modal dialog with InputText at top, scrollable list below
- Hotkey: **Ctrl+P** (checked each frame via `ImGui::IsKeyPressed(ImGuiKey_P)` with `KeyCtrl`)

## Files
- `src/editor/command_palette.h` — pure API (fuzzy_score, filter_commands, CommandRegistry, register_builtin_commands)
- `src/editor/command_palette.cpp` — implementation
- `src/editor/editor.cpp` — Ctrl+P handling + modal drawing in draw_frame()
- `tests/unit/test_command_palette.cpp` — 6 tests (fuzzy_score, filter_commands, register_builtin_commands)

## Tests
```
FuzzyScoreEmptyQueryReturnsPositive
FuzzyScoreExactMatchBeatsSubstring
FuzzyScoreSubstringReturnsPositive
FuzzyScoreNoMatchReturnsZero
FuzzyScoreCaseInsensitive
FilterCommandsEmptyQueryReturnsAll
FilterCommandsSortedByScoreDescending
FilterCommandsNoMatches
RegisterBuiltinCommandsPopulatesRegistry
```

## Notes
- Commands that are incomplete (e.g., "File: New Scene") log a message and are deferred to later batches
- The fuzzy scorer is pure (no side effects) and can be tested in isolation
- Invoking a command mutates EditorState (not pure) but the palette itself is a thin wrapper
