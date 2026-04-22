#pragma once

// ---------------------------------------------------------------------------
// command_palette.h
// Ctrl+P command palette for the editor (Batch H).
//
// Provides:
//   - CommandItem: descriptor for a single command (id, label, keybind, invoke).
//   - fuzzy_score: pure fuzzy-matching algorithm (0 = no match, higher = better).
//   - filter_commands: pure filtering + sorting by fuzzy score.
//   - CommandRegistry: canonical command list (editable, bound to EditorState).
//   - register_builtin_commands: populate the registry with standard editor commands.
// ---------------------------------------------------------------------------

#include <functional>
#include <string>
#include <vector>

namespace odyssey::editor {

struct EditorState;

// Descriptor for a single command in the palette.
struct CommandItem {
    std::string id;         // Unique identifier (e.g. "file.new", "edit.undo")
    std::string label;      // Display label (e.g. "File: New Scene")
    std::string keybind;    // Display-only keybind string (e.g. "Ctrl+N")
    std::function<void(EditorState&)> invoke;  // Callback to execute the command
};

// Pure fuzzy-match scorer.
// Returns 0 if no match; higher values indicate better matches.
// Algorithm: char-by-char forward scan; bonus points for sequential matches + start-of-word.
// Both query and label are compared case-insensitively.
//
// Derivation:
//   - Base score: 1 point per matched char.
//   - Sequential bonus: 5 extra points when current match immediately follows previous.
//   - Start-of-word bonus: 10 extra points for matches at label start or after space/underscore.
//   - Early bonus: 1 point per unmatched char from label-start to first match (prefers early matches).
int fuzzy_score(const std::string& query, const std::string& label);

// Pure: filter + sort commands by fuzzy score.
// Returns a vector of pointers into the original items vector, sorted by fuzzy_score descending.
// Empty query returns all items in original order.
std::vector<const CommandItem*> filter_commands(
    const std::vector<CommandItem>& items,
    const std::string& query);

// Command registry: holds the canonical list of commands.
struct CommandRegistry {
    std::vector<CommandItem> items;
};

// Populate the registry with built-in editor commands (File, Edit, Hierarchy, etc.).
// This function binds the invoke lambdas, which are not pure (they mutate EditorState).
// Called once during Editor::initialize.
void register_builtin_commands(CommandRegistry& reg);

} // namespace odyssey::editor
