#include "editor/undo_stack.h"

#include <spdlog/spdlog.h>

namespace odyssey::editor {

void UndoStack::push(UndoEntry entry) {
    // Discard all redo history
    future_.clear();

    // Add to undo history
    past_.push_back(std::move(entry));

    // Trim to max depth (remove oldest)
    while (past_.size() > kMaxDepth) {
        past_.pop_front();
    }

    spdlog::debug("[undo_stack] pushed entry, depth now {} (undo={}, redo={})",
                  past_.size(), past_.size(), future_.size());
}

bool UndoStack::can_undo() const {
    return !past_.empty();
}

bool UndoStack::can_redo() const {
    return !future_.empty();
}

const UndoEntry* UndoStack::peek_undo() const {
    return past_.empty() ? nullptr : &past_.back();
}

const UndoEntry* UndoStack::peek_redo() const {
    return future_.empty() ? nullptr : &future_.back();
}

UndoEntry UndoStack::pop_undo() {
    UndoEntry entry = std::move(past_.back());
    past_.pop_back();
    future_.push_back(entry);  // Move to redo stack
    spdlog::debug("[undo_stack] popped undo, depth now (undo={}, redo={})",
                  past_.size(), future_.size());
    return entry;
}

UndoEntry UndoStack::pop_redo() {
    UndoEntry entry = std::move(future_.back());
    future_.pop_back();
    past_.push_back(entry);  // Move to undo stack
    spdlog::debug("[undo_stack] popped redo, depth now (undo={}, redo={})",
                  past_.size(), future_.size());
    return entry;
}

void UndoStack::clear() {
    past_.clear();
    future_.clear();
    spdlog::debug("[undo_stack] cleared");
}

} // namespace odyssey::editor
