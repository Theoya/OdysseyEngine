#include "editor/command_palette.h"
#include "editor/editor.h"

#include <algorithm>
#include <cctype>
#include <spdlog/spdlog.h>

namespace odyssey::editor {

// Pure fuzzy-match scorer.
// Returns 0 if query is empty or no match found; higher = better match.
int fuzzy_score(const std::string& query, const std::string& label) {
    if (query.empty()) {
        // Empty query matches everything (all commands)
        return 1;
    }

    int score = 0;
    size_t label_idx = 0;
    bool prev_matched = false;
    int unmatched_prefix = 0;

    for (size_t q_idx = 0; q_idx < query.length(); ++q_idx) {
        char q_char = std::tolower(static_cast<unsigned char>(query[q_idx]));

        // Scan label for this character
        bool found = false;
        while (label_idx < label.length()) {
            char l_char = std::tolower(static_cast<unsigned char>(label[label_idx]));
            if (l_char == q_char) {
                found = true;
                score += 1;  // Base: 1 point per matched char

                // Bonus: sequential match (immediately after previous match)
                if (prev_matched) {
                    score += 5;
                }

                // Bonus: start of word (after space, underscore, or at position 0)
                if (label_idx == 0 || label[label_idx - 1] == ' ' || label[label_idx - 1] == '_') {
                    score += 10;
                }

                // Bonus: early match (prefer matches near the start)
                score += std::max(0, 10 - (int)unmatched_prefix);

                label_idx++;  // Move past this char in label
                prev_matched = true;
                break;
            }
            unmatched_prefix++;
            label_idx++;
        }

        if (!found) {
            return 0;  // Query character not found in label — no match
        }
    }

    return score;
}

// Pure: filter + sort commands by fuzzy score.
std::vector<const CommandItem*> filter_commands(
    const std::vector<CommandItem>& items,
    const std::string& query) {

    std::vector<const CommandItem*> results;

    for (const auto& item : items) {
        int score = fuzzy_score(query, item.label);
        if (score > 0) {
            results.push_back(&item);
        }
    }

    // Sort by score descending (higher score first)
    std::sort(results.begin(), results.end(),
              [&items, &query](const CommandItem* a, const CommandItem* b) {
                  return fuzzy_score(query, a->label) > fuzzy_score(query, b->label);
              });

    return results;
}

// Register built-in commands.
void register_builtin_commands(CommandRegistry& reg) {
    // --- File Commands ---
    reg.items.push_back({
        "file.new", "File: New Scene", "Ctrl+N",
        [](EditorState& state) {
            spdlog::info("[editor] File: New Scene (not yet implemented in Batch H)");
        }
    });

    reg.items.push_back({
        "file.open", "File: Open Scene", "Ctrl+O",
        [](EditorState& state) {
            spdlog::info("[editor] File: Open Scene (deferred to Batch I)");
        }
    });

    reg.items.push_back({
        "file.save", "File: Save", "Ctrl+S",
        [](EditorState& state) {
            state.save_requested = true;
            spdlog::info("[editor] Save requested via command palette");
        }
    });

    reg.items.push_back({
        "file.save_as", "File: Save As", "Ctrl+Shift+S",
        [](EditorState& state) {
            spdlog::info("[editor] File: Save As (deferred to Batch I)");
        }
    });

    // --- Edit Commands ---
    reg.items.push_back({
        "edit.undo", "Edit: Undo", "Ctrl+Z",
        [](EditorState& state) {
            spdlog::info("[editor] Edit: Undo (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "edit.redo", "Edit: Redo", "Ctrl+Y",
        [](EditorState& state) {
            spdlog::info("[editor] Edit: Redo (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "edit.cut", "Edit: Cut", "Ctrl+X",
        [](EditorState& state) {
            spdlog::info("[editor] Edit: Cut (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "edit.copy", "Edit: Copy", "Ctrl+C",
        [](EditorState& state) {
            spdlog::info("[editor] Edit: Copy (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "edit.paste", "Edit: Paste", "Ctrl+V",
        [](EditorState& state) {
            spdlog::info("[editor] Edit: Paste (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "edit.duplicate", "Edit: Duplicate", "Ctrl+D",
        [](EditorState& state) {
            spdlog::info("[editor] Edit: Duplicate (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "edit.delete", "Edit: Delete", "Del",
        [](EditorState& state) {
            spdlog::info("[editor] Edit: Delete (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "edit.select_all", "Edit: Select All", "Ctrl+A",
        [](EditorState& state) {
            spdlog::info("[editor] Edit: Select All (deferred to Batch K)");
        }
    });

    // --- Hierarchy Commands ---
    reg.items.push_back({
        "hierarchy.create_empty", "Hierarchy: Create Empty", "",
        [](EditorState& state) {
            if (state.entities) {
                EntityID new_id = state.entities->create_entity("new_entity", "");
                state.selected_entity = new_id;
                spdlog::info("[editor] Created empty entity {}", new_id);
            }
        }
    });

    reg.items.push_back({
        "hierarchy.rename", "Hierarchy: Rename", "F2",
        [](EditorState& state) {
            spdlog::info("[editor] Hierarchy: Rename (deferred to Batch K)");
        }
    });

    // --- Inspector Commands ---
    reg.items.push_back({
        "inspector.add_component", "Inspector: Add Component", "",
        [](EditorState& state) {
            spdlog::info("[editor] Inspector: Add Component (deferred to Batch K)");
        }
    });

    // --- Viewport Commands ---
    reg.items.push_back({
        "viewport.frame_selected", "Viewport: Frame Selected", "F",
        [](EditorState& state) {
            spdlog::info("[editor] Viewport: Frame Selected (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "viewport.tool_move", "Viewport: Move Tool", "W",
        [](EditorState& state) {
            state.gizmo_mode = GizmoMode::Translate;
            spdlog::info("[editor] Move tool activated via command palette");
        }
    });

    reg.items.push_back({
        "viewport.tool_rotate", "Viewport: Rotate Tool", "E",
        [](EditorState& state) {
            state.gizmo_mode = GizmoMode::Rotate;
            spdlog::info("[editor] Rotate tool activated via command palette");
        }
    });

    reg.items.push_back({
        "viewport.tool_scale", "Viewport: Scale Tool", "R",
        [](EditorState& state) {
            state.gizmo_mode = GizmoMode::Scale;
            spdlog::info("[editor] Scale tool activated via command palette");
        }
    });

    reg.items.push_back({
        "viewport.toggle_snap", "Viewport: Toggle Snap", "S",
        [](EditorState& state) {
            state.snap_enabled = !state.snap_enabled;
            spdlog::info("[editor] Snap toggled: {}", state.snap_enabled);
        }
    });

    reg.items.push_back({
        "viewport.toggle_local_world", "Viewport: Toggle Local/World", "",
        [](EditorState& state) {
            state.gizmo_space = (state.gizmo_space == GizmoSpace::Local)
                ? GizmoSpace::World : GizmoSpace::Local;
            spdlog::info("[editor] Gizmo space toggled");
        }
    });

    // --- Layout Commands ---
    reg.items.push_back({
        "layout.default", "Layout: Default", "",
        [](EditorState& state) {
            spdlog::info("[editor] Layout: Default (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "layout.tall", "Layout: Tall", "",
        [](EditorState& state) {
            spdlog::info("[editor] Layout: Tall (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "layout.wide", "Layout: Wide", "",
        [](EditorState& state) {
            spdlog::info("[editor] Layout: Wide (deferred to Batch K)");
        }
    });

    // --- Window Commands ---
    reg.items.push_back({
        "window.toggle_hierarchy", "Window: Toggle Hierarchy", "",
        [](EditorState& state) {
            spdlog::info("[editor] Window: Toggle Hierarchy (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "window.toggle_inspector", "Window: Toggle Inspector", "",
        [](EditorState& state) {
            spdlog::info("[editor] Window: Toggle Inspector (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "window.toggle_viewport", "Window: Toggle Viewport", "",
        [](EditorState& state) {
            spdlog::info("[editor] Window: Toggle Viewport (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "window.toggle_asset_browser", "Window: Toggle Asset Browser", "",
        [](EditorState& state) {
            spdlog::info("[editor] Window: Toggle Asset Browser (deferred to Batch K)");
        }
    });

    reg.items.push_back({
        "window.toggle_log", "Window: Toggle Log", "",
        [](EditorState& state) {
            spdlog::info("[editor] Window: Toggle Log (deferred to Batch K)");
        }
    });

    // --- Mode Commands ---
    reg.items.push_back({
        "mode.edit", "Mode: Edit", "",
        [](EditorState& state) {
            state.mode = Mode::Edit;
            spdlog::info("[editor] Mode changed to Edit via command palette");
        }
    });

    reg.items.push_back({
        "mode.play", "Mode: Play", "",
        [](EditorState& state) {
            state.mode = Mode::Play;
            spdlog::info("[editor] Mode changed to Play via command palette");
        }
    });

    reg.items.push_back({
        "mode.simulate", "Mode: Simulate", "",
        [](EditorState& state) {
            state.mode = Mode::Simulate;
            spdlog::info("[editor] Mode changed to Simulate via command palette");
        }
    });

    // --- Help Commands ---
    reg.items.push_back({
        "help.about", "Help: About", "",
        [](EditorState& state) {
            spdlog::info("[editor] Help: About (opening dialog)");
            // TODO: emit flag to show about dialog in editor.cpp draw_frame
        }
    });

    reg.items.push_back({
        "help.architecture", "Help: Architecture", "",
        [](EditorState& state) {
            spdlog::info("[editor] Help: Open Architecture doc (deferred to Batch I)");
        }
    });

    reg.items.push_back({
        "help.nadir_guide", "Help: Nadir Guide", "",
        [](EditorState& state) {
            spdlog::info("[editor] Help: Open Nadir Guide (deferred to Batch I)");
        }
    });

    reg.items.push_back({
        "help.cli_reference", "Help: CLI Reference", "",
        [](EditorState& state) {
            spdlog::info("[editor] Help: Open CLI Reference (deferred to Batch I)");
        }
    });
}

} // namespace odyssey::editor
